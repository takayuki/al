open Cil
module P = Pretty
module E = Errormsg
module F = Formatcil

let atomic_list = ref ([] : global list)
let lock_list = ref ([] : (string * varinfo) list)
let type_list = ref ([] : (string * typ) list)
let symbol_list = ref ([] : (string * varinfo) list)

let findLock name = List.assoc name !lock_list
let findType name =
  try List.assoc name !type_list
  with Not_found -> E.s (E.bug "type '%s' is not found in runtime" name)
let findSymbol name =
  try List.assoc name !symbol_list
  with Not_found -> E.s (E.bug "symbol '%s' is not found in runtime" name)
let _profile_t () = findType "_profile_t"

class collectGlobals = object
  inherit nopCilVisitor

  method vglob g = match g with
  | GType(t,_) when t.tname = "_profile_t" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GType(t,_) when t.tname = "_thread_t" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GType(t,_) when t.tname = "Thread" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxLoad" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxStore" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxStoreLocal" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "al" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "mallocInStm" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "freeInStm" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "mallocInLock" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "freeInLock" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "dump_profile" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | _ -> DoChildren
end

let current_func = ref (emptyFunction "")
let assignment_list = ref []

class locked = object
  inherit nopCilVisitor

  method vfunc f =
    current_func := f; DoChildren

  method vlval lv =
    match lv with
      Var v,NoOffset when
        v.vglob && isFunctionType v.vtype ->
        (match v.vname with
           "malloc" -> ChangeTo (var (findSymbol "mallocInLock"))
         | "free" -> ChangeTo (var (findSymbol "freeInLock"))
         | _ -> SkipChildren)
    | _ -> SkipChildren
end

let hasIntPtrSize (ts : typ list) =
  let fold = List.fold_left (fun x y -> x && y) true in
  let test t = bitsSizeOf t = bitsSizeOf intPtrType in
    fold (List.map test ts)

class transactional ((thread_self,ro) : varinfo * bool) = object
  inherit nopCilVisitor

  method vfunc f =
    current_func := f; DoChildren

  method unqueueInstr () =
    let result = List.rev !assignment_list in
    assignment_list := [] ;
    result

  method vinst i =
    match i with
    | Set((Var v,_),_,loc) when
      v.vglob && ro -> E.s (E.error
"assignment in read-only atomic section in %a." d_loc loc)
    | Set(((Var v,_) as lv),e,loc) when
      hasIntPtrSize [typeOfLval lv; (typeOf e)] ->
        let store = if v.vglob then Lval(var (findSymbol "TxStore"))
                    else Lval(var (findSymbol "TxStoreLocal")) in
        let arg = [e] in
        let inst = Call(None,store,arg,loc) in
        let protect x =
          match x with
            Call(None,proc,arg,loc)
              when proc == store ->
              let arg' = [Lval(var thread_self);mkAddrOf lv] @ arg in
                Call(None,proc,arg',loc)
          | _ -> x
        in ChangeDoChildrenPost ([inst],fun x -> List.map protect x)
    | Set(((Mem _,_) as lv),e,loc) when
      hasIntPtrSize [typeOfLval lv; (typeOf e)] ->
        let store = Lval(var (findSymbol "TxStore")) in
        let arg = [e] in
        let inst = Call(None,store,arg,loc) in
        let protect x =
          match x with
            Call(None,proc,arg,loc)
              when proc == store ->
              let arg' = [Lval(var thread_self);mkAddrOf lv] @ arg in
                Call(None,proc,arg',loc)
          | _ -> x
        in ChangeDoChildrenPost ([inst],fun x -> List.map protect x)
    | Set(lv,_,loc) when
      hasIntPtrSize [typeOfLval lv] -> E.s (E.error
"assignment to a non-interger-sized variable in %a." d_loc loc)
    | Set(_,_,loc) -> E.s (E.error
"assignment from a non-interger-sized value in %a." d_loc loc)
    | Call(None,_,_,_) -> DoChildren
    | Call(Some((Var v,_) as lv),arg,f,loc)
        when v.vglob ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval lv) in
          ChangeTo([Call(Some(var t),arg,f,loc);Set(lv,Lval(var t),loc)])
    | Call(Some(Var _,_),_,_,_) -> DoChildren
    | Call(Some((Mem _,_) as mem),arg,f,loc) ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval mem) in
          ChangeTo([Call(Some(var t),arg,f,loc);Set(mem,Lval(var t),loc)])
    | Asm _ -> SkipChildren

  method vlval lv =
    match lv with
      Var v,NoOffset when v.vglob && isFunctionType v.vtype ->
        (match v.vname with
           "malloc" -> ChangeTo (var (findSymbol "mallocInStm"))
         | "free" -> ChangeTo (var (findSymbol "freeInStm"))
         | _ -> SkipChildren)
    | Var v,_ when v.vglob && hasIntPtrSize [typeOfLval lv] ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval lv) in
        let load = findSymbol "TxLoad" in
        let arg = [Lval(var thread_self);mkAddrOf lv] in
        let loc = v.vdecl in
        let inst = Call(Some (var t),Lval(var load),arg,loc) in
         (assignment_list := inst :: !assignment_list; ChangeTo (var t))
    | Var v,_ when v.vglob -> 
        E.s (E.error "global variable `%s' has non-pointer size in %a"
                      v.vname d_loc !currentLoc)
    | Var _,_ -> SkipChildren
    | Mem e,_ ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval lv) in
        let load = findSymbol "TxLoad" in
        let arg = [Lval(var thread_self);e] in
        let inst = Call(Some (var t),Lval(var load),arg,locUnknown) in
         (assignment_list := inst :: !assignment_list; ChangeTo (var t))

  method vexpr e =
    match e with
    | SizeOfE _ -> SkipChildren
    | AlignOfE _ -> SkipChildren
    | AddrOf _ -> SkipChildren
    | StartOf _ -> SkipChildren
    | _ -> DoChildren
end

let compileRaw (f : fundec) =
  visitCilFunction (new locked) f

let compileStm ((f,self,ro) : fundec * varinfo * bool) =
  visitCilFunction (new transactional (self,ro)) f

let buildAl ((f,l,vs,ro) : fundec * location * (varinfo list) * bool) =
  begin
    f.slocals <- [];
    let t = makeTempVar f ~name:"tmp" voidPtrType in
    begin
     f.slocals <- [t];
     let arg = (List.map (fun v -> mkAddrOf(var v)) vs) @
               (List.map (fun v -> Lval(var v)) f.sformals) @
               [if ro then one else zero] in
     let s1 = mkStmtOneInstr
              (Call(Some (var t),Lval(var (findSymbol "al")),arg,l)) in
     let s2 = mkStmt (Return(Some(Lval(var t)),l)) in
     let body = mkBlock [s1;s2] in
       f.sbody <- body
    end
  end

let buildInit ((f,loc,v) : fundec * location * varinfo) =
  let base = f.svar.vname in
  let g = emptyFunction ("_init_"^base) in
  setFunctionType g (TFun(TVoid[Attr("constructor",[])],Some[],false,[]));
  let f = match unrollType v.vtype with
            TComp(ci,_) -> getCompField ci "name"
          | _ -> E.s (E.bug "profile should have composite type") in
  let s = mkStmtOneInstr
          (Set((Var v,Field(f,NoOffset)),Const(CStr(v.vname)),loc)) in
  let body = mkBlock [s] in
    g.sbody <- body; GFun(g,loc)

let buildAtExit ((f,loc,v) : fundec * location * varinfo) =
  let base = f.svar.vname in
  let g = emptyFunction ("_atexit_"^base) in
  setFunctionType g (TFun(TVoid[Attr("destructor",[])],Some[],false,[]));
  let arg = [mkAddrOf(var v)] in
  let s = mkStmtOneInstr
          (Call(None,Lval(var (findSymbol "dump_profile")),arg,loc)) in
  let body = mkBlock [s] in
    g.sbody <- body; GFun(g,loc)

let findAtomic (g : global) =
  match g with
    GFun(f,loc) ->
     (match f.svar.vtype with
        TFun(ret,_,_,_) ->
          let attr = let rec unrollAttr tt =
                       (match tt with
                          TPtr(t,_) -> (typeAttrs tt) @ (unrollAttr t)
                        | _ -> typeAttrs tt) in
                     f.svar.vattr @ unrollAttr ret in
          if hasAttribute "atomic" attr then
            let decl = GVarDecl(f.svar,loc) in
            atomic_list := decl :: !atomic_list;
            let base = f.svar.vname in
            let lock,ro = match (filterAttributes "atomic" attr) with
                            [Attr(_,[])] -> base,false
                          | [Attr(_,[AStr "ro"])] -> base,true
                          | [Attr(_,[AStr param])] -> param,false
                          | [Attr(_,[AStr param;AStr "ro"])] -> param,true
                          | _ -> E.s (E.error
"syntax error in atomic attribute in %a." d_loc loc) in
            let p,q = try findLock lock,[]
                      with Not_found ->
                      (let p = makeGlobalVar
                               ("_"^lock^"_prof") (_profile_t()) in
                       lock_list := (lock,p) :: !lock_list;
                       let c = buildInit (f,loc,p) in
                       let d = buildAtExit (f,loc,p) in
                       (p,[GVar(p,{init=None},loc);c;d])) in
            let r = copyFunction f ("_raw_"^base) in
            let s = copyFunction f ("_stm_"^base) in
            let t = TPtr(findType "Thread",[]) in
            let v = makeFormalVar s ~where:"^" "self" t in
              ignore(compileRaw r);
              ignore(compileStm (s,v,ro));
              buildAl (f,loc,[p;r.svar;s.svar],ro);
              q@[GFun(r,loc);GFun(s,loc);g]
          else
            [g]
      | _ -> E.s (E.bug "function should have function type"))
  | _ -> [g]

class dropAttrAtomic = object
  inherit nopCilVisitor

  method vattr attr =
    ChangeTo (dropAttribute "atomic" [attr])
end

let doatomic (f : file) =
  begin
    visitCilFileSameGlobals (new collectGlobals) f;
    f.globals <- List.concat (List.map findAtomic f.globals);
    visitCilFileSameGlobals (new dropAttrAtomic) f;
  end

let feature : featureDescr = {
  fd_name = "atomic";
  fd_enabled = ref false;
  fd_description = "implement atomic section with adaptive lock";
  fd_extraopt = [];
  fd_doit = doatomic;
  fd_post_check = true;
}
