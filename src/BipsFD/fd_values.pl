/*-------------------------------------------------------------------------*/
/* GNU Prolog                                                              */
/*                                                                         */
/* Part  : FD constraint solver buit-in predicates                         */
/* File  : fd_values.pl                                                    */
/* Descr.: FD variable values management                                   */
/* Author: Daniel Diaz                                                     */
/*                                                                         */
/* Copyright (C) 1999,2000 Daniel Diaz                                     */
/*                                                                         */
/* GNU Prolog is free software; you can redistribute it and/or modify it   */
/* under the terms of the GNU General Public License as published by the   */
/* Free Software Foundation; either version 2, or any later version.       */
/*                                                                         */
/* GNU Prolog is distributed in the hope that it will be useful, but       */
/* WITHOUT ANY WARRANTY; without even the implied warranty of              */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        */
/* General Public License for more details.                                */
/*                                                                         */
/* You should have received a copy of the GNU General Public License along */
/* with this program; if not, write to the Free Software Foundation, Inc.  */
/* 59 Temple Place - Suite 330, Boston, MA 02111, USA.                     */
/*-------------------------------------------------------------------------*/

:- built_in_fd.

'$use_fd_values'.


fd_domain(List,R):-
        set_bip_name(fd_domain,2),
	'$call_c_test'('Fd_Domain_2'(List,R)).




fd_domain(List,L,U):-
        set_bip_name(fd_domain,3),
	'$call_c_test'('Fd_Domain_3'(List,L,U)).


'$fd_domain'(X,L,U):-                        % for fd builtins (exact errors)
	fd_tell(fd_domain(X,L,U)).




fd_domain_bool(List):-
        set_bip_name(fd_domain_bool,1),
        '$call_c_test'('Fd_Domain_Bool_1'(List)).




fd_labeling(List):-
        set_bip_name(fd_labeling,1),
	'$fd_labeling'(List,[]).


fd_labelingff(List):-
	set_bip_name(fd_labelingff,1),
	'$fd_labeling'(List,[variable_method(first_fail)]).




fd_labeling(List,Options):-
	set_bip_name(fd_labeling,2),
	'$fd_labeling'(List,Options).


'$fd_labeling'(List,Options):-
	'$set_labeling_defaults',
	'$get_labeling_options'(Options,Bckts),
	'$sys_var_read'(0,VarMethod),
	'$sys_var_read'(1,ValMethod),
	'$sys_var_read'(2,Reorder),
	'$sys_var_write'(3,0),                                % bckts counter
	((fd_var(List) ; integer(List))
            -> '$indomain'(List,ValMethod)
            ;  '$check_list'(List),
               '$fd_labeling1'(List,VarMethod,ValMethod,Reorder)),
	'$sys_var_read'(3,Bckts),
	'$sys_var_write'(3,0).                          % reset bckts counter




'$set_labeling_defaults':-
	'$sys_var_write'(0,0),
	'$sys_var_write'(1,0),
	'$sys_var_write'(2,1).




'$get_labeling_options'(Options,Bckts):-
	'$check_list'(Options),
	g_link('$backtracks',_),
	'$get_labeling_options1'(Options),
	g_read('$backtracks',Bckts).


'$get_labeling_options1'([]).

'$get_labeling_options1'([X|Options]):-
        '$get_labeling_options2'(X),
        !,
        '$get_labeling_options1'(Options).


'$get_labeling_options2'(X):-
        var(X),
        '$pl_err_instantiation'.

'$get_labeling_options2'(variable_method(X)):-
	nonvar(X),                           % same order as in fd_values_c.c
	(X=standard,         '$sys_var_write'(0,0)
             ;
	 X=first_fail,       '$sys_var_write'(0,1)
             ;
	 X=ff,               '$sys_var_write'(0,1)
             ;
	 X=most_constrained, '$sys_var_write'(0,2)
             ;
	 X=smallest,         '$sys_var_write'(0,3)
             ;
	 X=largest,          '$sys_var_write'(0,4)
             ;
	 X=max_regret,       '$sys_var_write'(0,5)
             ;
	 X=random,           '$sys_var_write'(0,6)).

'$get_labeling_options2'(value_method(X)):-
	nonvar(X),                           % same order as in fd_values_c.c
	(X=min,    '$sys_var_write'(1,0)
             ;
	 X=max,    '$sys_var_write'(1,1)
             ;
	 X=middle, '$sys_var_write'(1,2)
             ;
	 X=limits, '$sys_var_write'(1,3)
             ;
	 X=random, '$sys_var_write'(1,4)).

'$get_labeling_options2'(reorder(X)):-
	nonvar(X),
	(X=false,            '$sys_var_write'(2,0)
             ;
         X=true,             '$sys_var_write'(2,1)).

'$get_labeling_options2'(backtracks(Bckts)):-
	g_link('$backtracks',Bckts).

'$get_labeling_options2'(X):-
        '$pl_err_domain'(fd_labeling_option,X).




'$fd_labeling1'(List,0,ValMethod,_):-                              % standard
	!,
	'$fd_labeling_std'(List,ValMethod).

'$fd_labeling1'(List,VarMethod,ValMethod,Reorder):-
	'$fd_sel_array_from_list'(List,SelArray),
	'$fd_labeling_mth'(SelArray,VarMethod,ValMethod,Reorder).






'$fd_labeling_std'([],_).

'$fd_labeling_std'([X|List],ValMethod):-
	'$indomain'(X,ValMethod),
	'$fd_labeling_std'(List,ValMethod).




'$fd_labeling_mth'(SelArray,VarMethod,ValMethod,Reorder):-
	'$fd_sel_array_pick_var'(SelArray,VarMethod,Reorder,X),
	!,
	'$indomain'(X,ValMethod),
	'$fd_labeling_mth'(SelArray,VarMethod,ValMethod,Reorder).

'$fd_labeling_mth'(_,_,_,_).




'$fd_sel_array_from_list'(List,SelArray):-
	'$call_c_test'('Fd_Sel_Array_From_List_2'(List,SelArray)).




'$fd_sel_array_pick_var'(SelArray,Method,Reorder,Fdv):-
	'$call_c_test'('Fd_Sel_Array_Pick_Var_4'(SelArray,Method,Reorder,Fdv)).




'$indomain'(X,ValMethod):-
	'$call_c_test'('Indomain_2'(X,ValMethod)).


'$indomain_min_alt':-               % used by C code to create a choice-point
        '$call_c_test'('Indomain_Min_Alt_0').

'$indomain_max_alt':-               % used by C code to create a choice-point
        '$call_c_test'('Indomain_Max_Alt_0').

'$indomain_middle_alt':-            % used by C code to create a choice-point
        '$call_c_test'('Indomain_Middle_Alt_0').

'$indomain_limits_alt':-            % used by C code to create a choice-point
        '$call_c_test'('Indomain_Limits_Alt_0').

'$indomain_random_alt':-            % used by C code to create a choice-point
        '$call_c_test'('Indomain_Random_Alt_0').

'$extra_cstr_alt':-                 % used by C code to create a choice-point
        '$call_c_test'('Extra_Cstr_Alt_0').
