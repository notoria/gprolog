/*-------------------------------------------------------------------------*
 * GNU Prolog                                                              *
 *                                                                         *
 * Part  : Prolog to WAM compiler                                          *
 * File  : internal.pl                                                     *
 * Descr.: pass 2: internal format transformation                          *
 * Author: Daniel Diaz                                                     *
 *                                                                         *
 * Copyright (C) 1999,2000 Daniel Diaz                                     *
 *                                                                         *
 * GNU Prolog is free software; you can redistribute it and/or modify it   *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2, or any later version.       *
 *                                                                         *
 * GNU Prolog is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
 * General Public License for more details.                                *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc.  *
 * 59 Temple Place - Suite 330, Boston, MA 02111, USA.                     *
 *-------------------------------------------------------------------------*/

/* $Id$ */

/*-------------------------------------------------------------------------*
 * predicate internal format: (I(t)=internal format of t)                  *
 *                                                                         *
 * I(p(Arg1,...,ArgN))= p(NoPred,Pred/N,[I(Arg1),...,I(ArgN)])             *
 *                                                                         *
 * NoPred : predicate number = corresponding chunk number                  *
 *                                                                         *
 * Pred/N : predicate/arity                                                *
 *                                                                         *
 * I(Argi): internal format of the ith argument                            *
 *                                                                         *
 *    var          : var(VarName,Info) with:                               *
 *                                                                         *
 *                   VarName=x(NoX) temporary (in pass 2 NoX is unbound or *
 *                                  assigned to void if var is singleton)  *
 *                           y(NoY) permanent (in pass 2 NoY is assigned)  *
 *                   Info   =in_heap       : the var is stored in the heap *
 *                           unsafe        : the var refers cur env.       *
 *                           not_in_cur_env: the var doesn't reside in the *
 *                                           current environment           *
 *                           in pass 2 Info or remains unbound             *
 *                                                                         *
 *    atom []      : nil                                                   *
 *    atom (others): atm(atom)                                             *
 *    integer      : int(integer)                                          *
 *    float        : flt(float)                                            *
 *    f(A1,...,An) : stc(f,n,[I(A1),...,I(An)])  ([H|T] = '.'(H,T))        *
 *                                                                         *
 * NB: true/0 in the body of a clause is removed.                          *
 *     variables are classified and permanent variables are assigned       *
 *     (temporary=x(_), permanent=y(i))                                    *
 *-------------------------------------------------------------------------*/

internal_format(Head, Body, Head1, Body1, NbChunk, NbY) :-
	format_head(Head, DicoVar, Head1),
	format_body(Body, DicoVar, Body1, NbChunk),
	classif_vars(DicoVar, 0, NbY).




format_head(Head, DicoVar, Head1) :-
	format_pred(Head, 0, DicoVar, Head1, _).




format_body(Body, DicoVar, Body1, NbChunk) :-
	format_body1(Body, 0, DicoVar, t, [], Body1, NbChunk, _).

format_body1((P,Q), NoPred, DicoVar, StartChunk, LNext, P1, NoPred2, StartChunk2) :-
	!,
	format_body1(P, NoPred, DicoVar, StartChunk, Q1, P1, NoPred1, StartChunk1),
	format_body1(Q, NoPred1, DicoVar, StartChunk1, LNext, Q1, NoPred2, StartChunk2).

format_body1(true, NoPred, _, StartChunk, LNext, LNext, NoPred, StartChunk) :-
	!.

format_body1(Pred, NoPred, DicoVar, StartChunk, LNext, [Pred1|LNext], NoPred1, StartChunk1) :-
	(   StartChunk=t ->
	    NoPred1 is NoPred+1
	;   NoPred1=NoPred
	),
	format_pred(Pred, NoPred1, DicoVar, Pred1, InlinePred),
	(   InlinePred=t ->
	    StartChunk1=f
	;   StartChunk1=t
	).




format_pred(Pred, NoPred, DicoVar, p(NoPred, F/N, ArgLst1), InlinePred) :-
	functor(Pred, F, N),
	Pred=..[_|ArgLst],
	format_arg_lst(ArgLst, NoPred, DicoVar, ArgLst1),
	(   inline_predicate(F, N) ->
	    InlinePred=t
	;   InlinePred=f
	).




format_arg_lst([], _, _, []).

format_arg_lst([Arg|ArgLst], NoPred, DicoVar, [Arg1|ArgLst1]) :-
	format_arg(Arg, NoPred, DicoVar, Arg1), !,
	format_arg_lst(ArgLst, NoPred, DicoVar, ArgLst1).




format_arg(Var, NoPred, DicoVar, V) :-
	var(Var),
	add_var_to_dico(DicoVar, Var, NoPred, V).

format_arg([], _, _, nil).

format_arg(A, _, _, atm(A)) :-
	atom(A).

format_arg(N, _, _, int(N)) :-
	integer(N).

format_arg(N, _, _, flt(N)) :-
	float(N).

format_arg(Fonc, NoPred, DicoVar, stc(F, N, ArgLst1)) :-
	functor(Fonc, F, N),
	Fonc=..[_|ArgLst],
	format_arg_lst(ArgLst, NoPred, DicoVar, ArgLst1).




          % DicoVar=[ v(Var,NoPred1stOcc,Singleton,V), ... | EndVar ]
          %
          % Singleton = f or unbound variable
          % V=var(VarName,VarInfo)
          % VarName=x(_) or y(_)
          % Info=unbound or singleton

add_var_to_dico(DicoVar, Var, NoPred1stOcc, V) :-
	var(DicoVar), !,
	V=var(_, _),
	DicoVar=[v(Var, NoPred1stOcc, _, V)|_].

add_var_to_dico([v(Var1, NoPred1stOcc1, Singleton, V)|_], Var2, NoPred1stOcc2, V) :-
	Var1==Var2, !,
	V=var(VarName, _),
	Singleton=f,
	(   var(VarName),
	    NoPred1stOcc1\==NoPred1stOcc2,
	    NoPred1stOcc2>1 ->
	    VarName=y(_)
	;   true
	).

add_var_to_dico([_|DicoVar], Var, NoPred1stOcc, V) :-
	add_var_to_dico(DicoVar, Var, NoPred1stOcc, V).




classif_vars([], NbY, NbY) :-
	!.

classif_vars([v(_, _, Singleton, var(VarName, _))|DicoVar], Y, NbY) :-
	var(VarName), !,
	(   var(Singleton) ->
	    VarName=x(void)
	;   VarName=x(_)
	),
	classif_vars(DicoVar, Y, NbY).

classif_vars([v(_, _, _, var(y(Y), _))|DicoVar], Y, NbY) :-
	Y1 is Y+1,
	classif_vars(DicoVar, Y1, NbY).




	% Inline predicates: inline_predicate(Pred,Arity)
	% all predicates defined here must have a corresponding clause
	% gen_inline_pred/5 in pass 3 describing their associated code

inline_predicate(Pred, Arity) :-
	g_read(inline, Inline),
	inline_predicate(Pred, Arity, Inline).




inline_predicate('$get_cut_level', 1, _).
inline_predicate('$cut', 1, _).




inline_predicate(CallC, 1, _) :-                 % must be an inline predicate
	(   CallC='$call_c'
	;   CallC='$call_c_test'
	;   CallC='$call_c_jump'
	), !,
	test_call_c_allowed(CallC/1).

inline_predicate(=, 2, _).

inline_predicate('$foreign_call_c', 7, _).


inline_predicate(var, 1, t).

inline_predicate(nonvar, 1, t).

inline_predicate(atom, 1, t).

inline_predicate(integer, 1, t).

inline_predicate(float, 1, t).

inline_predicate(number, 1, t).

inline_predicate(atomic, 1, t).

inline_predicate(compound, 1, t).

inline_predicate(callable, 1, t).

inline_predicate(list, 1, t).

inline_predicate(partial_list, 1, t).

inline_predicate(list_or_partial_list, 1, t).


inline_predicate(fd_var, 1, t).

inline_predicate(non_fd_var, 1, t).

inline_predicate(generic_var, 1, t).

inline_predicate(non_generic_var, 1, t).




inline_predicate(functor, 3, t).

inline_predicate(arg, 3, t).

inline_predicate(compare, 3, t).

inline_predicate(=.., 2, t).



inline_predicate(==, 2, t).

inline_predicate(\==, 2, t).

inline_predicate(@<, 2, t).

inline_predicate(@=<, 2, t).

inline_predicate(@>, 2, t).

inline_predicate(@>=, 2, t).




inline_predicate(is, 2, t).

inline_predicate(=:=, 2, t).

inline_predicate(=\=, 2, t).

inline_predicate(<, 2, t).

inline_predicate(=<, 2, t).

inline_predicate(>, 2, t).

inline_predicate(>=, 2, t).




inline_predicate(g_assign, 2, t).

inline_predicate(g_assignb, 2, t).

inline_predicate(g_link, 2, t).

inline_predicate(g_read, 2, t).

inline_predicate(g_array_size, 2, t).
