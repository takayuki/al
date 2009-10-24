open Cil
module P = Pretty
module E = Errormsg
module F = Formatcil
open Printf

let atomic_list = ref ([] : global list)
let lock_list = ref ([] : (string * varinfo) list)
let type_list = ref ([] : (string * typ) list)
let func_list = ref ([] : (string * fundec) list)
let symbol_list = ref ([] : (string * varinfo) list)

let findLock name = List.assoc name !lock_list
let findType name =
  try List.assoc name !type_list
  with Not_found -> E.s (E.bug "type '%s' is not found in runtime" name)
let findFunc name =
  try List.assoc name !func_list
  with Not_found -> E.s (E.bug "func '%s' is not found in runtime" name)
let findSymbol name =
  try List.assoc name !symbol_list
  with Not_found -> E.s (E.bug "symbol '%s' is not found in runtime" name)
let profile_t () = findType "profile_t"

class collectGlobals = object
  inherit nopCilVisitor

  method vglob g = match g with
  | GType(t,_) when t.tname = "intptr_t" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GType(t,_) when t.tname = "profile_t" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GType(t,_) when t.tname = "thread_t" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GType(t,_) when t.tname = "Thread" ->
      (type_list := (t.tname,TNamed(t,[])) :: !type_list; SkipChildren)
  | GFun(f,_) when f.svar.vname = "_al_template" ->
      (func_list := (f.svar.vname,f) :: !func_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxLoadSized" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxStoreSized" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "al" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxAlloc" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "TxFree" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "tmalloc_reserve" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "tmalloc_release" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | GVarDecl(v,_) when v.vname = "dump_profile" ->
      (symbol_list := (v.vname,v) :: !symbol_list; SkipChildren)
  | _ -> DoChildren
end

let alignedIntPtrSize (ts : typ list) =
  let fold = List.fold_left (fun x y -> x && y) true in
  let test t = ((bitsSizeOf t) mod (bitsSizeOf intPtrType)) = 0 in
    fold (List.map test ts)

let castIntPtr e =
  let p = findType "intptr_t" in
    mkCast ~e ~newt:p

let castIntPPtr e =
  let p = findType "intptr_t" in
  let pp = TPtr(p,[]) in
    mkCast ~e ~newt:pp

let castVIntPPtr e =
  let p = typeAddAttributes [Attr("volatile",[])] (findType "intptr_t") in
  let pp = TPtr(p,[]) in
    mkCast ~e ~newt:pp

let current_func = ref (emptyFunction "")
let assignment_list = ref []

class locked = object
  inherit nopCilVisitor

  method vfunc f =
    current_func := f; DoChildren

  method vlval lv =
    match lv with
      Var v,NoOffset
      when v.vglob && isFunctionType v.vtype && v.vname = "malloc" ->
        ChangeTo (var (findSymbol "tmalloc_reserve"))
    | Var v,NoOffset
      when v.vglob && isFunctionType v.vtype && v.vname = "free" ->
        ChangeTo (var (findSymbol "tmalloc_release"))
    | _ -> SkipChildren
end

class transact ((thread_self,ro) : varinfo * bool) = object
  inherit nopCilVisitor

  method vfunc f =
    current_func := f; DoChildren

  method unqueueInstr () =
    let result = List.rev !assignment_list in
    assignment_list := [] ;
    result

  method vinst i =
    match i with
    | Set((Var v,_),_,_) when not v.vglob -> DoChildren
    | Set((Var v,_),_,loc) when v.vglob && ro ->
      E.s (E.error "update '%s' in read-only atomic section in %a"
                   v.vname d_loc loc)
    | Set(((Var v,_) as lv),e,loc)
      when v.vglob && alignedIntPtrSize [typeOfLval lv; (typeOf e)] ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOf e) in
        let store = Lval(var (findSymbol "TxStoreSized")) in
        let arg = [castIntPPtr(mkAddrOf(lv));
                   castIntPPtr(mkAddrOf(var t));
                   SizeOf(typeOf e)] in
        let inst = [Set(var t,e,loc);Call(None,store,arg,loc)] in
        let protect x =
          match x with
            Call(None,proc,arg,loc)
            when proc == store ->
              let arg' = [Lval(var thread_self)] @ arg in
                Call(None,proc,arg',loc)
          | _ -> x
        in ChangeDoChildrenPost (inst,fun x -> List.map protect x)
    | Set(((Mem _,_) as lv),e,loc)
      when alignedIntPtrSize [typeOfLval lv; (typeOf e)] ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOf e) in
        let store = Lval(var (findSymbol "TxStoreSized")) in
        let arg = [castIntPPtr(mkAddrOf(lv));
                   castIntPPtr(mkAddrOf(var t));
                   SizeOf(typeOf e)] in
        let inst = [Set(var t,e,loc);Call(None,store,arg,loc)] in
        let protect x =
          match x with
            Call(None,proc,arg,loc) when proc == store ->
              let arg' = [Lval(var thread_self)] @ arg in
                Call(None,proc,arg',loc)
          | _ -> x
        in ChangeDoChildrenPost (inst,fun x -> List.map protect x)
    | Set(lv,_,loc) ->
        E.s (E.error "transact update '%a' is to be aligned in %a"
                      d_lval lv d_loc loc)
    | Call(lv,Lval(Var v,NoOffset),arg,loc)
      when v.vname = "malloc" ->
        let f = Lval(var (findSymbol "TxAlloc")) in
        let arg = Lval(var thread_self) :: arg in
          ChangeDoChildrenPost([Call(lv,f,arg,loc)],fun x -> x)
    | Call(lv,Lval(Var v,NoOffset),arg,loc)
      when v.vname = "free" ->
        let f = Lval(var (findSymbol "TxFree")) in
        let arg = Lval(var thread_self) :: arg in
          ChangeDoChildrenPost([Call(lv,f,arg,loc)],fun x -> x)
    | Call(None,_,_,_) -> DoChildren
    | Call(Some((Var v,_) as lv),f,arg,loc)
      when v.vglob ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval lv) in
        let inst = [Call(Some(var t),f,arg,loc);Set(lv,Lval(var t),loc)] in
        let revist = visitCilInstr (new transact (thread_self,ro)) in
          ChangeTo (List.concat (List.map revist inst))
    | Call(Some(Var _,_),_,_,_) -> DoChildren
    | Call(Some((Mem _,_) as lv),f,arg,loc) ->
        let t = makeTempVar !current_func ~name:"tmp" (typeOfLval lv) in
        let inst = [Call(Some(var t),f,arg,loc);Set(lv,Lval(var t),loc)] in
        let revist = visitCilInstr (new transact (thread_self,ro)) in
          ChangeTo (List.concat (List.map revist inst))
    | Asm _ -> SkipChildren

  method vlval lv =
    match lv with
      Var v,NoOffset when isFunctionType v.vtype -> SkipChildren
    | Var v,_ when v.vglob ->
        let ext = function
          lv when alignedIntPtrSize [typeOfLval lv] ->
            let t = makeTempVar !current_func ~name:"var" (typeOfLval lv) in
            let load = findSymbol "TxLoadSized" in
            let arg = [Lval(var thread_self);
                       castIntPPtr(mkAddrOf(var t));
                       castIntPPtr(mkAddrOf(lv));
                       SizeOf(typeOfLval lv)] in
            let loc = v.vdecl in
            let inst = Call(None,Lval(var load),arg,loc) in
              (assignment_list := inst :: !assignment_list;var t)
        | _ -> E.s (E.error "'%a' is not aligned in %a"
                            d_lval lv d_loc !currentLoc)
        in ChangeDoChildrenPost(lv,ext)
    | Var _,_ -> SkipChildren
    | Mem _,_ ->
        let ext = function
          lv when alignedIntPtrSize [typeOfLval lv] ->
            let t = makeTempVar !current_func ~name:"mem" (typeOfLval lv) in
            let load = findSymbol "TxLoadSized" in
            let arg = [Lval(var thread_self);
                       castIntPPtr(mkAddrOf(var t));
                       castIntPPtr(mkAddrOf(lv));
                       SizeOf(typeOfLval lv)] in
            let inst = Call(None,Lval(var load),arg,locUnknown) in
              (assignment_list := inst :: !assignment_list;var t)
        | _ -> E.s (E.error "'%a' is not aligned in %a"
                            d_lval lv d_loc !currentLoc)
        in ChangeDoChildrenPost(lv,ext)

  method vexpr e =
    match e with
      SizeOfE _ -> SkipChildren
    | AlignOfE _ -> SkipChildren
    | AddrOf lv ->
      (match lv with
         Var _,_ -> SkipChildren
       | Mem e,off -> ChangeDoChildrenPost(e,fun x -> AddrOf(Mem x,off)))
    | StartOf _ -> SkipChildren
    | _ -> DoChildren
end

let compileRaw (f : fundec) =
  visitCilFunction (new locked) f

let compileStm ((f,self,ro) : fundec * varinfo * bool) =
  visitCilFunction (new transact (self,ro)) f

class tweakAl ((vs,ro,ret) : varinfo list * bool * varinfo list) = object
  inherit nopCilVisitor

  method vfunc f =
    current_func := f; DoChildren

  method vstmt s =
    match s.skind with
      Return(None,loc) ->
       ((match ret with [v] ->
           s.skind <- Return(Some(Lval(var v)),loc)
         | _ -> ());
       ChangeTo(s))
    | _ -> DoChildren

  method vinst i =
    let prefix = "" in
    match vs with
      [prof;rawfunc;stmfunc] ->
      (match i with
       | Set((Var v,NoOffset),_,loc) when v.vname = (prefix^"prof") ->
           ChangeTo([Set((Var v,NoOffset),mkAddrOf(var prof),loc)])
       | Set((Var v,NoOffset),_,loc) when v.vname = (prefix^"ro") && ro ->
           ChangeTo([Set((Var v,NoOffset),one,loc)])
       | Set((Var v,NoOffset),_,loc) when v.vname = (prefix^"ro") && not ro ->
           ChangeTo([Set((Var v,NoOffset),zero,loc)])
       | Call(None,Lval(Mem(Lval(Var v,NoOffset)),NoOffset),arg,loc)
         when v.vname = (prefix^"stmfunc") ->
           let arg' = List.map (fun v -> Lval(var v))
                               (!current_func).sformals in
           let ret = match ret with [v] -> Some(var v) | _ -> None in
             ChangeTo([Call(ret,Lval(Var stmfunc,NoOffset),arg@arg',loc)])
       | Call(None,Lval(Mem(Lval(Var v,NoOffset)),NoOffset),arg,loc)
         when v.vname = (prefix^"rawfunc") ->
           let arg' = List.map (fun v -> Lval(var v))
                               (!current_func).sformals in
           let ret = match ret with [v] -> Some(var v) | _ -> None in
             ChangeTo([Call(ret,Lval(Var rawfunc,NoOffset),arg@arg',loc)])
       | _ -> DoChildren)
    | _ -> E.s (E.bug "invalid number of arguments")
end

let buildAl ((f,vs,ro) : fundec * (varinfo list) * bool) =
  let f' = copyFunction (findFunc "_al_template") f.svar.vname in
  let ret = match f.svar.vtype with
              TFun(TVoid _,_,_,_) -> []
            | TFun(r,_,_,_) -> [makeTempVar f' ~name:"tmp" r]
            | _ -> E.s (E.bug "template should have function type") in
  begin
    f'.svar.vtype <- f.svar.vtype;
    f'.sformals <- f.sformals;
    visitCilFunction (new tweakAl (vs,ro,ret)) f'
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

let findAtomic = function
  GFun(f,loc) as g ->
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
                        | _ -> E.s (E.error "'%a' has invalid attribute in %a"
                                    d_attrlist attr d_loc loc) in
          let p,q = try findLock lock,[]
                    with Not_found ->
                    (let p = makeGlobalVar
                             ("_"^lock^"_prof") (profile_t()) in
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
            let f' = buildAl(f,[p;r.svar;s.svar],ro) in
              q@[GFun(r,loc);GFun(s,loc);GFun(f',loc)]
        else [g]
      | _ -> E.s (E.bug "'%a' should have function type" d_global g))
| g -> [g]

class dropAttrAtomic = object
  inherit nopCilVisitor

  method vattr attr =
    ChangeTo (dropAttribute "atomic" [attr])
end

let dropTemplate = function
  GFun(f,_) when f.svar.vname = "_al_template" -> false
| _ -> true

let doatomic (f : file) =
  begin
    visitCilFileSameGlobals (new collectGlobals) f;
    f.globals <- List.concat (List.map findAtomic f.globals);
    visitCilFileSameGlobals (new dropAttrAtomic) f;
    f.globals <- List.filter dropTemplate f.globals
  end

let feature : featureDescr = {
  fd_name = "atomic";
  fd_enabled = ref false;
  fd_description = "implement atomic section with adaptive lock";
  fd_extraopt = [];
  fd_doit = doatomic;
  fd_post_check = true;
}
