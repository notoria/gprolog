/*-------------------------------------------------------------------------*
 * GNU Prolog                                                              *
 *                                                                         *
 * Part  : Prolog buit-in predicates                                       *
 * File  : src_rdr_c.c                                                     *
 * Descr.: Prolog source file reader - C part                              *
 * Author: Daniel Diaz                                                     *
 *                                                                         *
 * Copyright (C) 1999-2002 Daniel Diaz                                     *
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

#include <string.h>

#include "engine_pl.h"
#include "bips_pl.h"

/*---------------------------------*
 * Constants                       *
 *---------------------------------*/

#define DO                         0
#define UNDO                       1


#define NO_INCLUDE_LIST            NULL
#define CURRENT_INCLUDE_LIST       (SrIncWr *) 1




/*---------------------------------*
 * Type Definitions                *
 *---------------------------------*/

typedef enum
{
  OP,
  SET_PROLOG_FLAG,
  CHAR_CONVERSION
} SRDirType;


typedef struct sr_one_direct *PSROneDirect;

typedef struct sr_one_direct
{
  SRDirType type;		/* directive type */
  WamWord a[2][3];		/* arguments: a[DO][...] and a[UNDO][...] */
  PSROneDirect next;		/* forward link (or NULL if last) */
  PSROneDirect prev;		/* backward link (or NULL if first) */
} SROneDirect;


typedef struct
{
  SROneDirect *first;		/* first directive or NULL */
  SROneDirect *last;		/* last directive or NULL */
} SRDirect;



typedef struct sr_file *PSRFile;

typedef struct sr_file
{
  int atom_file_name;		/* file name atom */
  int stm;			/* associated stream # */
  Bool eof_reached;		/* is the EOF reached for this file ? */
  int include_line;		/* line # this file includes a child file */
  PSRFile parent_file;		/* link to the parent file (includer) */
} SRFile;




typedef struct sr_module *PSRModule;

typedef struct sr_module
{
  int atom_module_name;		/* module atom */
  int i_atom_file_def;		/* interface: file name of definition */
  int i_line_def;		/* interface: line # of definition */
  int b_atom_file_def;		/* body: file name of current body (or -1) */
  int b_line_def;		/* body: line # of current body */
  SRDirect direct_lst;		/* list of directives (interface + body) */
  PSRModule next;		/* next module */
} SRModule;




typedef struct
{
  Bool in_use;			/* open ? */
  Bool close_master_at_end;	/* close master at sr_close/1 ? */
  int mask;			/* see src_rdr.pl */
  SRFile *file_top;		/* stack of open files (top = current) */
  int cur_l1, cur_l2;		/* position (lines) of last read term */
  int char_count;		/* nb chars read in processed files */
  int line_count;		/* nb lines read in processed files */
  int error_count;		/* nb of errors emitted */
  int warning_count;		/* nb of warnings emitted */
  int out_sora_word;		/* SorA for writing (or NOT_A_WAM_WORD) */
  SRDirect direct_lst;		/* list of directives */
  SRModule *module_lst;		/* list of defined modules */
  SRModule *cur_module;		/* NULL or current module */
  Bool interface;		/* in interface or body of current module */
} SRInf;




/*---------------------------------*
 * Global Variables                *
 *---------------------------------*/

static SRInf *sr_tbl = NULL;	/* table (mallocated) */
static int sr_tbl_size = 0;	/* allocated size */
static int sr_last_used = -1;	/* last sr used */
static SRInf *cur_sr;		/* the current sr entry used */




/*---------------------------------*
 * Function Prototypes             *
 *---------------------------------*/


static SRInf *Get_Descriptor(WamWord desc_word, Bool accept_none);

static void Do_Directives(SRDirect *direct);

static void Undo_Directives(SRDirect *direct);

static void Exec_One_Directive(SROneDirect *o, int do_undo);

static void Close_Current_Module(void);

static StmInf *Write_Location(WamWord sora_word, WamWord list_word,
			      int atom_file_name, int l1, int l2c);

static void Write_Message_Text(StmInf *pstm,  WamWord sora_word,
			       WamWord type_word,
			       WamWord format_word, WamWord args_word);

				/* from oper_c.c */
void Op_3(WamWord prec_word, WamWord specif_word, WamWord oper_word);

				/* form flag_c.c */
Bool Set_Prolog_Flag_2(WamWord flag_word, WamWord value_word);

				/* from read_c.c */
void Char_Conversion_2(WamWord in_char_word, WamWord out_char_word);

				/* from write_c.c */

void Write_2(WamWord sora_word, WamWord term_word);

				/* from format_c.c */

void Format_3(WamWord sora_word, WamWord format_word, WamWord args_word);



#define Interf_Body(interf) ((interf) ? "module" : "body")


#define SR_CURRENT_DESC_ALT        X2473725F63757272656E745F64657363726970746F725F616C74

Prolog_Prototype(SR_CURRENT_DESC_ALT, 0);




/*-------------------------------------------------------------------------*
 * SR_INIT_OPEN_2                                                          *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Init_Open_2(WamWord desc_word, WamWord out_sora_word)
{
  SRInf *sr;
  int desc;

  if (sr_tbl == NULL)		/* first allocation */
    {
      sr_tbl_size = 8;
      sr_last_used = -1;
      sr_tbl = (SRInf *) Calloc(sr_tbl_size, sizeof(SRInf));
    }

  for(desc = 0; desc < sr_tbl_size; desc++)
    if (!sr_tbl[desc].in_use)
      break;

  if (desc == sr_tbl_size)
    Extend_Array((char **) &sr_tbl, &sr_tbl_size, sizeof(SRInf), TRUE);

  if (desc > sr_last_used)
    sr_last_used = desc;

  sr = cur_sr = sr_tbl + desc;

  if (sr->file_top)		    /* to due a previous aborted sr_open/3 */
    {
      Free(sr->file_top);
      sr->file_top = NULL;
    }

  sr->mask = SYS_VAR_OPTION_MASK;

  sr->cur_l1 = sr->cur_l2 = 0;
  sr->char_count = 0;
  sr->line_count = 0;
  sr->error_count = 0;
  sr->warning_count = 0;

  if (sys_var[1])
    {
      Get_Stream_Or_Alias(out_sora_word, STREAM_CHECK_VALID);
      sr->out_sora_word = out_sora_word;
    }
  else
    sr->out_sora_word = NOT_A_WAM_WORD;

  sr->direct_lst.first = NULL;
  sr->direct_lst.last = NULL;

  sr->module_lst = NULL;
  sr->cur_module = NULL;
  sr->interface = FALSE;

  Get_Integer(desc, desc_word);
}




/*-------------------------------------------------------------------------*
 * SR_FINISH_OPEN_2                                                        *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Finish_Open_1(WamWord close_master_at_end_word)
{
  SRInf *sr = cur_sr;

  sr->in_use = 1;
  sr->close_master_at_end = Rd_Boolean(close_master_at_end_word);
}




/*-------------------------------------------------------------------------*
 * SR_OPEN_FILE_2                                                          *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Open_File_2(WamWord file_name_word, WamWord stm_word)
{
  SRInf *sr = cur_sr;
  SRFile *file = (SRFile *) Malloc(sizeof(SRFile));

  file->atom_file_name = Rd_Atom(file_name_word);
  file->stm = Rd_Integer(stm_word);
  file->eof_reached = FALSE;
  file->parent_file = sr->file_top;

  if (sr->file_top)
    sr->file_top->include_line = sr->cur_l1;
  sr->file_top = file;
}




/*-------------------------------------------------------------------------*
 * SR_CLOSE_1                                                              *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Close_1(WamWord desc_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  SRFile *file, *file1;
  SROneDirect *o, *o1;
  SRModule *m, *m1;

  file = sr->file_top;
  if (!sr->close_master_at_end)
    goto skip_first;

  do
    {
      Close_Stm(file->stm, TRUE);
    skip_first:
      file1 = file;
      file = file->parent_file;
      Free(file1);
    }
  while(file);
  sr->file_top = NULL;

  if ((sr->mask & (1 << 18)) != 0)
    {
      Undo_Directives(&sr->direct_lst);
      if (sr->cur_module)
	Undo_Directives(&sr->cur_module->direct_lst);
    }

  o = sr->direct_lst.first;
  while(o)
    {
      o1 = o;
      o = o->next;
      Free(o1);
    }

  m = sr->module_lst;
  while(m)
    {
      o = m->direct_lst.first;
      while(o)
	{
	  o1 = o;
	  o = o->next;
	  Free(o1);
	}
      m1 = m;
      m = m->next;
      Free(m);
    }

  sr->in_use = FALSE;
  sr->file_top = NULL;
  
  while(sr_last_used >= 0 && !sr_tbl[sr_last_used].in_use)
    sr_last_used--;
}




/*-------------------------------------------------------------------------*
 * SR_ADD_DIRECTIVE_7                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Add_Directive_7(WamWord type_word, 
		   WamWord d1_word, WamWord d2_word, WamWord d3_word,
		   WamWord u1_word, WamWord u2_word, WamWord u3_word)
{
  SRInf *sr = cur_sr;
  SRDirect *d;
  SROneDirect one, *o;
  WamWord word, tag_mask;

  if (sr->cur_module == NULL)
    d = &sr->direct_lst;
  else
    d = &(sr->cur_module->direct_lst);

  o = &one;
  o->type = Rd_Integer(type_word);

  DEREF(d1_word, word, tag_mask);
  o->a[0][0] = word;

  DEREF(d2_word, word, tag_mask);
  o->a[0][1] = word;

  DEREF(d3_word, word, tag_mask);
  o->a[0][2] = word;

  DEREF(u1_word, word, tag_mask);
  o->a[1][0] = word;

  DEREF(u2_word, word, tag_mask);
  o->a[1][1] = word;

  DEREF(u3_word, word, tag_mask);
  o->a[1][2] = word;

  o->next = NULL;
  o->prev = d->last;

  Exec_One_Directive(o, DO);
				/* if exec OK, allocate and add it in lst */

  o = (SROneDirect *) Malloc(sizeof(SROneDirect));
  *o = one;

  if (d->first == NULL)
    d->first = o;
  else
    d->last->next = o;

  d->last = o;
}




/*-------------------------------------------------------------------------*
 * DO_DIRECTIVES                                                           *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static void
Do_Directives(SRDirect *direct)
{
  SROneDirect *o;

  for (o = direct->first; o; o = o->next)
    Exec_One_Directive(o, DO);
}




/*-------------------------------------------------------------------------*
 * UNDO_DIRECTIVES                                                         *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static void
Undo_Directives(SRDirect *direct)
{
  SROneDirect *o;

  for (o = direct->last; o; o = o->prev)
    Exec_One_Directive(o, UNDO);
}



/*-------------------------------------------------------------------------*
 * EXEC_ONE_DIRECTIVE                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static void
Exec_One_Directive(SROneDirect *o, int do_undo)
{
  switch(o->type)
    {
    case OP:
      Op_3(o->a[do_undo][0], o->a[do_undo][1], o->a[do_undo][2]);
      break;

    case SET_PROLOG_FLAG:
      Set_Prolog_Flag_2(o->a[do_undo][0], o->a[do_undo][1]);
      break;

    case CHAR_CONVERSION:
      Char_Conversion_2(o->a[do_undo][0], o->a[do_undo][1]);
      break;
    }
}




/*-------------------------------------------------------------------------*
 * SR_CHANGE_OPTIONS_0                                                     *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Change_Options_0(void)
{
  SRInf *sr = cur_sr;
  sr->mask = SYS_VAR_OPTION_MASK;
}




/*-------------------------------------------------------------------------*
 * SR_GET_STM__FOR_READ_TERM_1                                              *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Get_Stm_For_Read_Term_1(WamWord stm_word)
{
  SRInf *sr = cur_sr;
  SRFile *file;

  for(;;)
    {
      file = sr->file_top;

      if (!file->eof_reached)
	break;
				/* a EOF is reached */

      if (file->parent_file == NULL)
	break;			/* never close the master stream */

      sr->char_count += stm_tbl[file->stm]->char_count;
      sr->line_count += stm_tbl[file->stm]->line_count;
      sr->file_top = file->parent_file;

      if ((sr->mask & (1 << 16)) == 0)
	Close_Stm(file->stm, TRUE);

      Free(file);
    }

  Get_Integer(sr->file_top->stm, stm_word);
}




/*-------------------------------------------------------------------------*
 * SR_EOF_REACHED_1                                                        *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_EOF_Reached_1(WamWord err_word)
{
  SRInf *sr = cur_sr;

  sr->file_top->eof_reached = TRUE;	/* delay close at next read */

  if (sr->file_top->parent_file == NULL)
    {
      if (sr->cur_module)
	{
	  sprintf(glob_buff, "end_%s(%s) not encoutered - assumed found",
		  Interf_Body(sr->interface),
		  atom_tbl[sr->cur_module->atom_module_name].name);

	  Close_Current_Module();
	  Un_String(glob_buff, err_word);
	}

      return TRUE;		/* always reflect EOF for master file */
    }

  return sr->mask & (1 << 17);
}




/*-------------------------------------------------------------------------*
 * SR_UPDATE_POSITION_0                                                    *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Update_Position_0(void)
{
  SRInf *sr = cur_sr;

  sr->cur_l1 = last_read_line;
  sr->cur_l2 = stm_tbl[sr->file_top->stm]->line_count;
  if (stm_tbl[sr->file_top->stm]->line_pos > 0)
    sr->cur_l2++;
}




/*-------------------------------------------------------------------------*
 * SR_START_MODULE_3                                                       *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Start_Module_3(WamWord module_name_word, WamWord interface_word,
		  WamWord err_word)
{
  SRInf *sr = cur_sr;
  int atom_module_name = Rd_Atom_Check(module_name_word);
  Bool interface = Rd_Boolean(interface_word);
  SRModule *m;

  *glob_buff = '\0';

  for(m = sr->module_lst; m; m = m->next)
    if (m->atom_module_name == atom_module_name)
      break;

  if (m == NULL)
    {
      if (!interface)
	{
	  sprintf(glob_buff, "module(%s) not encoutered - interface assumed empty",
		  atom_tbl[atom_module_name].name);
	}
      m = (SRModule *) Malloc(sizeof(SRModule));
      m->atom_module_name = atom_module_name;
      m->i_atom_file_def = sr->file_top->atom_file_name;
      m->i_line_def = sr->cur_l1;
      m->b_atom_file_def = -1;
      m->b_line_def = 0;
      m->direct_lst.first = NULL;
      m->direct_lst.last = NULL;
      m->next = sr->module_lst;
      sr->module_lst = m;
    }
  else
    {
      if (interface)
	{
	  sprintf(glob_buff, "module(%s) already found at %s:%d - directive ignored",
		  atom_tbl[atom_module_name].name,
		  atom_tbl[m->i_atom_file_def].name,
		  m->i_line_def);
	  goto error;
	}
    }

  if (sr->cur_module)
    {
      sprintf(glob_buff, "end_%s(%s) not encoutered - assumed found",
	      Interf_Body(sr->interface),
	      atom_tbl[sr->cur_module->atom_module_name].name);

      Close_Current_Module();
    }

  if (!interface)
    {
      m->b_atom_file_def = sr->file_top->atom_file_name;
      m->b_line_def = sr->cur_l1;
    }
  
  sr->cur_module = m;
  sr->interface = interface;

  Do_Directives(&m->direct_lst);

  if (*glob_buff)
    {
    error:
      Un_String(glob_buff, err_word);
    }
}




/*-------------------------------------------------------------------------*
 * SR_STOP_MODULE_3                                                        *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Stop_Module_3(WamWord module_name_word, WamWord interface_word,
		 WamWord err_word)
{
  SRInf *sr = cur_sr;
  int atom_module_name = Rd_Atom_Check(module_name_word);
  Bool interface = Rd_Boolean(interface_word);
  SRModule *m = sr->cur_module;

  *glob_buff = '\0';

  if (m == NULL)
    {
      sprintf(glob_buff, "corresponding directive %s(%s) not found - directive ignored",
	      Interf_Body(interface),
	      atom_tbl[atom_module_name].name);
      goto error;
    }

  if (interface != sr->interface || atom_module_name != m->atom_module_name)
    {
      sprintf(glob_buff, "directive mismatch wrt %s:%d - replaced by end_%s(%s)",
	      (sr->interface) ? 
	      atom_tbl[m->i_atom_file_def].name :
	      atom_tbl[m->b_atom_file_def].name,
	      (sr->interface) ? m->i_line_def : m->b_line_def,
	      Interf_Body(sr->interface),
	      atom_tbl[m->atom_module_name].name);
    }

  Close_Current_Module();

  if (*glob_buff)
    {
    error:
      Un_String(glob_buff, err_word);
    }
}




/*-------------------------------------------------------------------------*
 * CLOSE_CURRENT_MODULE                                                    *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static
void Close_Current_Module(void)
{
  SRInf *sr = cur_sr;

  Undo_Directives(&sr->cur_module->direct_lst);
  sr->cur_module = NULL;
}




/*-------------------------------------------------------------------------*
 * SR_CURRENT_DESCRIPTOR_1                                                 *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Current_Descriptor_1(WamWord desc_word)
{
  WamWord word, tag_mask;
  int desc = 0;

  DEREF(desc_word, word, tag_mask);
  if (tag_mask == TAG_INT_MASK)
    {
      desc = UnTag_INT(word);
      return (desc >= 0 && desc <= sr_last_used && sr_tbl[desc].in_use);
    }
  if (tag_mask != TAG_REF_MASK)
    Pl_Err_Type(type_integer, word);

  for (; desc <= sr_last_used; desc++)
    if (sr_tbl[desc].in_use)
      break;

  if (desc >= sr_last_used)
    {
      if (desc > sr_last_used)
	return FALSE;
    }
  else				/* non deterministic case */
    {
      A(0) = desc_word;
      A(1) = desc + 1;
      Create_Choice_Point((CodePtr) Prolog_Predicate(SR_CURRENT_DESC_ALT, 0),
			  2);
    }
  return Get_Integer(desc, desc_word);
}




/*-------------------------------------------------------------------------*
 * SR_CURRENT_DESCRIPTOR_ALT_0                                             *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Current_Descriptor_Alt_0(void)
{
  WamWord desc_word;
  int desc;

  Update_Choice_Point((CodePtr) Prolog_Predicate(SR_CURRENT_DESC_ALT, 0), 0);

  desc_word = AB(B, 0);
  desc = AB(B, 1);

  for (; desc <= sr_last_used; desc++)
    if (sr_tbl[desc].in_use)
      break;

  if (desc >= sr_last_used)
    {
      Delete_Last_Choice_Point();
      if (desc > sr_last_used)
	return FALSE;
    }
  else				/* non deterministic case */
    {
#if 0				/* the following data is unchanged */
      AB(B, 0) = desc_word;
#endif
      AB(B, 1) = desc + 1;
    }

  return Get_Integer(desc, desc_word);
}




/*-------------------------------------------------------------------------*
 * SR_IS_BIT_SET_1                                                         *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Is_Bit_Set_1(WamWord bit_word)
{
  return cur_sr->mask & (1 << Rd_Integer(bit_word));
}




/*-------------------------------------------------------------------------*
 * SR_GET_STM_2                                                            *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Stm_2(WamWord desc_word, WamWord stm_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  return Get_Integer(sr->file_top->stm, stm_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_MODULE_3                                                         *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Module_3(WamWord desc_word, WamWord module_name_word, 
		WamWord interface_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  SRModule *m = sr->cur_module;

  Check_For_Un_Atom(module_name_word);
  Check_For_Un_Atom(interface_word);

  if (m == NULL)
    return FALSE;

  if (!Get_Atom(m->atom_module_name, module_name_word))
    return FALSE;

  if (sr->interface)
    return Un_String("interface", interface_word);
  
  return Un_String("body", interface_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_FILE_NAME_2                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_File_Name_2(WamWord desc_word, WamWord file_name_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  return Un_Atom_Check(sr->file_top->atom_file_name, file_name_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_POSITION_3                                                       *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Position_3(WamWord desc_word, WamWord l1_word, WamWord l2_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);

  Check_For_Un_Integer(l1_word);
  Check_For_Un_Integer(l2_word);

  return Get_Integer(sr->cur_l1, l1_word) &&
    Get_Integer(sr->cur_l2, l2_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_INCLUDE_LIST_2                                                   *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Include_List_2(WamWord desc_word, WamWord list_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  SRFile *file;
  WamWord word;

  Check_For_Un_List(list_word);

				/* skip 1st file (current) */
  for(file = sr->file_top->parent_file; file; file = file->parent_file)
    {
      word = Put_Structure(ATOM_CHAR(':'), 2);
      Unify_Atom(file->atom_file_name);
      Unify_Integer(file->include_line);

      if (!Get_List(list_word) || !Unify_Value(word))
        return FALSE;

      list_word = Unify_Variable();
    }

  return Get_Nil(list_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_INCLUDE_STREAM_LIST_2                                            *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Include_Stream_List_2(WamWord desc_word, WamWord list_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  SRFile *file;
  WamWord word;

  Check_For_Un_List(list_word);

				/* skip 1st file (current) */
  for(file = sr->file_top->parent_file; file; file = file->parent_file)
    {
      word = Put_Structure(atom_stream, 1);
      Unify_Integer(file->stm);

      if (!Get_List(list_word) || !Unify_Value(word))
        return FALSE;

      list_word = Unify_Variable();
    }

  return Get_Nil(list_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_SIZE_COUNTERS_3                                                  *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Size_Counters_3(WamWord desc_word, WamWord chars_word, 
		       WamWord lines_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  SRFile *file;
  int char_count, line_count;

  Check_For_Un_Integer(chars_word);
  Check_For_Un_Integer(lines_word);

  char_count = sr->char_count;
  line_count = sr->line_count;

  for(file = sr->file_top;file ; file = file->parent_file)
    {
      char_count += stm_tbl[file->stm]->char_count;
      line_count += stm_tbl[file->stm]->line_count;
    }

  return Get_Integer(char_count, chars_word) &&
    Get_Integer(line_count, lines_word);
}




/*-------------------------------------------------------------------------*
 * SR_GET_ERROR_COUNTERS_3                                                 *
 *                                                                         *
 *-------------------------------------------------------------------------*/
Bool
SR_Get_Error_Counters_3(WamWord desc_word, WamWord errors_word, 
			WamWord warnings_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  
  Check_For_Un_Integer(errors_word);
  Check_For_Un_Integer(warnings_word);

  return Get_Integer(sr->error_count, errors_word) &&
    Get_Integer(sr->warning_count, warnings_word);
}




/*-------------------------------------------------------------------------*
 * SR_SET_ERROR_COUNTERS_3                                                 *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Set_Error_Counters_3(WamWord desc_word, WamWord errors_word, 
			WamWord warnings_word)
{
  SRInf *sr = Get_Descriptor(desc_word, FALSE);
  int errors = Rd_Integer_Check(errors_word);
  int warnings = Rd_Integer_Check(warnings_word);
  
  sr->error_count = errors;
  sr->warning_count = warnings;
}




/*-------------------------------------------------------------------------*
 * SR_CHECK_DESCRIPTOR_1                                                   *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Check_Descriptor_1(WamWord desc_word)
{
  Get_Descriptor(desc_word, FALSE);
}




/*-------------------------------------------------------------------------*
 * GET_DESCRIPTOR                                                          *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static SRInf *
Get_Descriptor(WamWord desc_word, Bool accept_none)
{
  WamWord word, tag_mask;
  int desc;
  int atom;

  if (accept_none)
    {
      DEREF(desc_word, word, tag_mask);
      atom = UnTag_ATM(word);
      if (tag_mask == TAG_ATM_MASK && 
	  strcmp(atom_tbl[atom].name, "none") == 0)
	{
	  cur_sr = NULL;
	  return cur_sr;
	}
      
    }
  desc = Rd_Integer_Check(desc_word);

  if ((unsigned) desc > sr_last_used || !sr_tbl[desc].in_use)
    Pl_Err_Existence(existence_sr_descriptor, desc_word);

  cur_sr = sr_tbl + desc;
  SYS_VAR_OPTION_MASK = cur_sr->mask;
  return cur_sr;
}




/*-------------------------------------------------------------------------*
 * SR_WRITE_MESSAGE_4                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Write_Message_4(WamWord desc_word,
		   WamWord type_word, WamWord format_word, WamWord args_word)
{
  SRInf *sr = Get_Descriptor(desc_word, TRUE);
  StmInf *pstm;
  int atom_file_name;
  int l1, l2c;
  WamWord sora_word;

  if (sr)
    {
      sora_word = sr->out_sora_word;
      atom_file_name = sr->file_top->atom_file_name;
      l1 = sr->cur_l1;
      l2c = sr->cur_l2;
    }
  else
    {
      sora_word = NOT_A_WAM_WORD;
      atom_file_name = atom_void;
      l1 = 0;
      l2c = 0;
    }

  pstm = Write_Location(sora_word, NOT_A_WAM_WORD, atom_file_name, l1, l2c);
  Write_Message_Text(pstm, sora_word, type_word, format_word, args_word);
}




/*-------------------------------------------------------------------------*
 * SR_WRITE_MESSAGE_6                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Write_Message_6(WamWord desc_word,
		   WamWord l1_word, WamWord l2c_word,
		   WamWord type_word, WamWord format_word, WamWord args_word)
{
  SRInf *sr = Get_Descriptor(desc_word, TRUE);
  StmInf *pstm;
  int atom_file_name;
  int l1, l2c;
  WamWord sora_word;

  if (sr)
    {
      sora_word = sr->out_sora_word;
      atom_file_name = sr->file_top->atom_file_name;
    }
  else
    {
      sora_word = NOT_A_WAM_WORD;
      atom_file_name = atom_void;
    }

  l1 = Rd_Integer_Check(l1_word);
  l2c = Rd_Integer_Check(l2c_word);
  
  pstm = Write_Location(sora_word, NOT_A_WAM_WORD, atom_file_name, l1, l2c);
  Write_Message_Text(pstm, sora_word, type_word, format_word, args_word);
}




/*-------------------------------------------------------------------------*
 * SR_WRITE_MESSAGE_8                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
void
SR_Write_Message_8(WamWord desc_word, WamWord list_word, 
		   WamWord file_name_word,
		   WamWord l1_word, WamWord l2c_word,
		   WamWord type_word, WamWord format_word, WamWord args_word)
{
  SRInf *sr = Get_Descriptor(desc_word, TRUE);
  StmInf *pstm;
  int atom_file_name;
  int l1, l2c;
  WamWord sora_word;

  if (!Blt_List(list_word))
    Pl_Err_Type(type_list, list_word);
  
  if (sr)
    {
      sora_word = sr->out_sora_word;
    }
  else
    {
      sora_word = NOT_A_WAM_WORD;
    }

  sora_word = sr->out_sora_word;
  atom_file_name = Rd_Atom_Check(file_name_word);
  l1 = Rd_Integer_Check(l1_word);
  l2c = Rd_Integer_Check(l2c_word);
  
  pstm = Write_Location(sora_word, list_word, atom_file_name, l1, l2c);
  Write_Message_Text(pstm, sora_word, type_word, format_word, args_word);
}




/*-------------------------------------------------------------------------*
 * WRITE_LOCATION                                                          *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static StmInf *
Write_Location(WamWord sora_word, WamWord list_word, int atom_file_name,
	       int l1, int l2c)

{
  WamWord word, tag_mask;
  int stm;
  StmInf *pstm;
  WamWord *lst_adr;
  Bool first;
  SRInf *sr = cur_sr;
  SRFile *file = NULL;
  int char_count;

  stm = (sora_word == NOT_A_WAM_WORD)
    ? stm_output : Get_Stream_Or_Alias(sora_word, STREAM_CHECK_OUTPUT);
  pstm = stm_tbl[stm];

  last_output_sora = sora_word;
  Check_Stream_Type(stm, TRUE, FALSE);

  if (list_word != NOT_A_WAM_WORD && sr != NULL)
    file = sr->file_top->parent_file;

  for (first = TRUE; ; first = FALSE)
    {
      if (list_word != NOT_A_WAM_WORD)
	{
	  DEREF(list_word, word, tag_mask);

	  if (word == NIL_WORD)
	    break;
	}
      else
	if (file == NULL)
	  break;
	

      if (first)
	Stream_Puts("In file included from ", pstm);
      else
	Stream_Printf(pstm, ",\n%*s from ", 16, "");

      if (list_word != NOT_A_WAM_WORD)
	{
	  lst_adr = UnTag_LST(word);
				/* accepts sora_word = NOT_A_WAM_WORD */
	  Write_2(sora_word, Car(lst_adr));
	  list_word = Cdr(lst_adr);
	}
      else
	{
	  Stream_Printf(pstm, "%s:%d", 
			atom_tbl[file->atom_file_name].name,
			file->include_line);
	  file = file->parent_file;
	}
    }

  if (!first)
    Stream_Puts(":\n", pstm);


  char_count = pstm->char_count;

  if (atom_file_name != atom_void)
    Stream_Puts(atom_tbl[atom_file_name].name, pstm);

  if (l1 > 0)
    {
      Stream_Printf(pstm, ":%d", l1);

      if (l2c != l1)
	{
	  if (l2c > 0)
	    Stream_Printf(pstm, "--%d", l2c);
	  else
	    Stream_Printf(pstm, ":%d", -l2c);
	}
    }

  if (char_count != pstm->char_count)
    Stream_Puts(": ", pstm);

  return pstm;
}




/*-------------------------------------------------------------------------*
 * WRITE_MESSAGE_TEXT                                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/
static void
Write_Message_Text(StmInf *pstm, WamWord sora_word,
		   WamWord type_word, WamWord format_word, WamWord args_word)
{
  SRInf *sr = cur_sr;
  char *type = Rd_String_Check(type_word);

  if (*type)
    {
      Stream_Printf(pstm, "%s: ", type);
      if (sr)
	{
	  if (strstr(type, "error") || strstr(type, "exception"))
	    sr->error_count++;
	  else if (strstr(type, "warning"))
	    sr->warning_count++;
	}
    }
				/* accepts sora_word = NOT_A_WAM_WORD */
  Format_3(sora_word, format_word, args_word); 
}
