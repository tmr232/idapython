#ifndef __PY_TYPEINF__
#define __PY_TYPEINF__

//<inline(py_typeinf)>
//-------------------------------------------------------------------------
PyObject *idc_parse_decl(til_t *ti, const char *decl, int flags)
{
  tinfo_t tif;
  qstring name;
  qtype fields, type;
  bool ok = parse_decl2(ti, decl, &name, &tif, flags);
  if ( ok )
    ok = tif.serialize(&type, &fields, NULL, SUDT_FAST);

  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( ok )
    return Py_BuildValue("(sss)",
                         name.c_str(),
                         (char *)type.c_str(),
                         (char *)fields.c_str());
  Py_RETURN_NONE;
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def calc_type_size(ti, tp):
    """
    Returns the size of a type
    @param ti: Type info. 'idaapi.cvar.idati' can be passed.
    @param tp: type string
    @return:
        - None on failure
        - The size of the type
    """
    pass
#</pydoc>
*/
PyObject *py_calc_type_size(const til_t *ti, PyObject *tp)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( PyString_Check(tp) )
  {
    // To avoid release of 'data' during Py_BEGIN|END_ALLOW_THREADS section.
    borref_t tpref(tp);
    const type_t *data = (type_t *)PyString_AsString(tp);
    size_t sz;
    Py_BEGIN_ALLOW_THREADS;
    tinfo_t tif;
    tif.deserialize(ti, &data, NULL, NULL);
    sz = tif.get_size();
    Py_END_ALLOW_THREADS;
    if ( sz != BADSIZE )
      return PyInt_FromLong(sz);
    Py_RETURN_NONE;
  }
  else
  {
    PyErr_SetString(PyExc_ValueError, "String expected!");
    return NULL;
  }
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def apply_type(ti, ea, tp_name, py_type, py_fields, flags)
    """
    Apply the specified type to the address
    @param ti: Type info library. 'idaapi.cvar.idati' can be used.
    @param py_type: type string
    @param py_fields: fields string (may be empty or None)
    @param ea: the address of the object
    @param flags: combination of TINFO_... constants or 0
    @return: Boolean
    """
    pass
#</pydoc>
*/
static bool py_apply_type(til_t *ti, PyObject *py_type, PyObject *py_fields, ea_t ea, int flags)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) )
  {
    PyErr_SetString(PyExc_ValueError, "Typestring must be passed!");
    return NULL;
  }
  const type_t *type   = (const type_t *) PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);
  bool rc;
  Py_BEGIN_ALLOW_THREADS;
  struc_t *sptr;
  member_t *mptr = get_member_by_id(ea, &sptr);
  if ( type[0] == '\0' )
  {
    if ( mptr != NULL )
    {
      rc = mptr->has_ti();
      if ( rc )
        del_member_tinfo(sptr, mptr);
    }
    else
    {
      rc = has_ti(ea);
      if ( rc )
        del_tinfo2(ea);
    }
  }
  else
  {
    tinfo_t tif;
    rc = tif.deserialize(ti, &type, &fields, NULL);
    if ( rc )
    {
      if ( mptr != NULL )
        rc = set_member_tinfo2(sptr, mptr, 0, tif, 0);
      else
        rc = apply_tinfo2(ea, tif, flags);
    }
  }
  Py_END_ALLOW_THREADS;
  return rc;
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def print_type(ea, one_line):
    """
    Returns the type of an item
    @return:
        - None on failure
        - The type string with a semicolon. Can be used directly with idc.SetType()
    """
    pass
#</pydoc>
*/
static PyObject *py_print_type(ea_t ea, bool one_line)
{
  qstring out;
  int flags = PRTYPE_SEMI | (one_line ? PRTYPE_1LINE : PRTYPE_MULTI);
  bool ok = print_type3(&out, ea, one_line ? PRTYPE_1LINE : PRTYPE_MULTI);
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( ok )
    return PyString_FromString(out.begin());
  Py_RETURN_NONE;
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def py_unpack_object_from_idb(ti, tp, fields, ea, pio_flags = 0):
    """
    Unpacks from the database at 'ea' to an object.
    Please refer to unpack_object_from_bv()
    """
    pass
#</pydoc>
*/
PyObject *py_unpack_object_from_idb(
  til_t *ti,
  PyObject *py_type,
  PyObject *py_fields,
  ea_t ea,
  int pio_flags = 0)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) )
  {
    PyErr_SetString(PyExc_ValueError, "Typestring must be passed!");
    return NULL;
  }

  // To avoid release of 'type'/'fields' during Py_BEGIN|END_ALLOW_THREADS section.
  borref_t py_type_ref(py_type);
  borref_t py_fields_ref(py_fields);

  // Unpack
  type_t *type   = (type_t *) PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);
  idc_value_t idc_obj;
  error_t err;
  Py_BEGIN_ALLOW_THREADS;
  err = unpack_object_from_idb(
      &idc_obj,
      ti,
      type,
      fields,
      ea,
      NULL,
      pio_flags);
  Py_END_ALLOW_THREADS;

  // Unpacking failed?
  if ( err != eOk )
    return Py_BuildValue("(ii)", 0, err);

  // Convert
  ref_t py_ret;
  err = idcvar_to_pyvar(idc_obj, &py_ret);

  // Conversion failed?
  if ( err != CIP_OK )
    return Py_BuildValue("(ii)", 0, err);
  else
    return Py_BuildValue("(iO)", 1, py_ret.o);
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def unpack_object_from_bv(ti, tp, fields, bytes, pio_flags = 0):
    """
    Unpacks a buffer into an object.
    Returns the error_t returned by idaapi.pack_object_to_idb
    @param ti: Type info. 'idaapi.cvar.idati' can be passed.
    @param tp: type string
    @param fields: fields string (may be empty or None)
    @param bytes: the bytes to unpack
    @param pio_flags: flags used while unpacking
    @return:
        - tuple(0, err) on failure
        - tuple(1, obj) on success
    """
    pass
#</pydoc>
*/
PyObject *py_unpack_object_from_bv(
  til_t *ti,
  PyObject *py_type,
  PyObject *py_fields,
  PyObject *py_bytes,
  int pio_flags = 0)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) || !PyString_Check(py_bytes) )
  {
    PyErr_SetString(PyExc_ValueError, "Incorrect argument type!");
    return NULL;
  }

  // To avoid release of 'type'/'fields' during Py_BEGIN|END_ALLOW_THREADS section.
  borref_t py_type_ref(py_type);
  borref_t py_fields_ref(py_fields);

  // Get type strings
  type_t *type   = (type_t *) PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);

  // Make a byte vector
  bytevec_t bytes;
  bytes.resize(PyString_Size(py_bytes));
  memcpy(bytes.begin(), PyString_AsString(py_bytes), bytes.size());

  idc_value_t idc_obj;
  error_t err;
  Py_BEGIN_ALLOW_THREADS;
  err = unpack_object_from_bv(
      &idc_obj,
      ti,
      type,
      fields,
      bytes,
      pio_flags);
  Py_END_ALLOW_THREADS;

  // Unpacking failed?
  if ( err != eOk )
    return Py_BuildValue("(ii)", 0, err);

  // Convert
  ref_t py_ret;
  err = idcvar_to_pyvar(idc_obj, &py_ret);

  // Conversion failed?
  if ( err != CIP_OK )
    return Py_BuildValue("(ii)", 0, err);

  return Py_BuildValue("(iO)", 1, py_ret.o);
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def pack_object_to_idb(obj, ti, tp, fields, ea, pio_flags = 0):
    """
    Write a typed object to the database.
    Raises an exception if wrong parameters were passed or conversion fails
    Returns the error_t returned by idaapi.pack_object_to_idb
    @param ti: Type info. 'idaapi.cvar.idati' can be passed.
    @param tp: type string
    @param fields: fields string (may be empty or None)
    @param ea: ea to be used while packing
    @param pio_flags: flags used while unpacking
    """
    pass
#</pydoc>
*/
PyObject *py_pack_object_to_idb(
  PyObject *py_obj,
  til_t *ti,
  PyObject *py_type,
  PyObject *py_fields,
  ea_t ea,
  int pio_flags = 0)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) )
  {
    PyErr_SetString(PyExc_ValueError, "Typestring must be passed!");
    return NULL;
  }

  // Convert Python object to IDC object
  idc_value_t idc_obj;
  borref_t py_obj_ref(py_obj);
  if ( !pyvar_to_idcvar_or_error(py_obj_ref, &idc_obj) )
    return NULL;

  // To avoid release of 'type'/'fields' during Py_BEGIN|END_ALLOW_THREADS section.
  borref_t py_type_ref(py_type);
  borref_t py_fields_ref(py_fields);

  // Get type strings
  type_t *type   = (type_t *)PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);

  // Pack
  // error_t err;
  error_t err;
  Py_BEGIN_ALLOW_THREADS;
  err = pack_object_to_idb(&idc_obj, ti, type, fields, ea, pio_flags);
  Py_END_ALLOW_THREADS;
  return PyInt_FromLong(err);
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def pack_object_to_bv(obj, ti, tp, fields, base_ea, pio_flags = 0):
    """
    Packs a typed object to a string
    @param ti: Type info. 'idaapi.cvar.idati' can be passed.
    @param tp: type string
    @param fields: fields string (may be empty or None)
    @param base_ea: base ea used to relocate the pointers in the packed object
    @param pio_flags: flags used while unpacking
    @return:
        tuple(0, err_code) on failure
        tuple(1, packed_buf) on success
    """
    pass
#</pydoc>
*/
// Returns a tuple(Boolean, PackedBuffer or Error Code)
PyObject *py_pack_object_to_bv(
  PyObject *py_obj,
  til_t *ti,
  PyObject *py_type,
  PyObject *py_fields,
  ea_t base_ea,
  int pio_flags=0)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) )
  {
    PyErr_SetString(PyExc_ValueError, "Typestring must be passed!");
    return NULL;
  }

  // Convert Python object to IDC object
  idc_value_t idc_obj;
  borref_t py_obj_ref(py_obj);
  if ( !pyvar_to_idcvar_or_error(py_obj_ref, &idc_obj) )
    return NULL;

  // To avoid release of 'type'/'fields' during Py_BEGIN|END_ALLOW_THREADS section.
  borref_t py_type_ref(py_type);
  borref_t py_fields_ref(py_fields);

  // Get type strings
  type_t *type   = (type_t *)PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);

  // Pack
  relobj_t bytes;
  error_t err;
  Py_BEGIN_ALLOW_THREADS;
  err = pack_object_to_bv(
    &idc_obj,
    ti,
    type,
    fields,
    &bytes,
    NULL,
    pio_flags);
  if ( err == eOk && !bytes.relocate(base_ea, inf.mf) )
      err = -1;
  Py_END_ALLOW_THREADS;
  if ( err == eOk )
    return Py_BuildValue("(is#)", 1, bytes.begin(), bytes.size());
  else
    return Py_BuildValue("(ii)", 0, err);
}

//-------------------------------------------------------------------------
/* Parse types from a string or file. See ParseTypes() in idc.py */
int idc_parse_types(const char *input, int flags)
{
  int hti = ((flags >> 4) & 7) << HTI_PAK_SHIFT;

  if ((flags & 1) != 0)
      hti |= HTI_FIL;

  return parse_decls(idati, input, (flags & 2) == 0 ? msg : NULL, hti);
}

//-------------------------------------------------------------------------
PyObject *py_idc_get_type_raw(ea_t ea)
{
  qtype type, fields;
  bool ok = get_tinfo(ea, &type, &fields);
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( ok )
    return Py_BuildValue("(ss)", (char *)type.c_str(), (char *)fields.c_str());
  else
    Py_RETURN_NONE;
}

//-------------------------------------------------------------------------
PyObject *py_idc_get_local_type_raw(int ordinal)
{
  const type_t *type;
  const p_list *fields;
  bool ok = get_numbered_type(idati, ordinal, &type, &fields);
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( ok )
    return Py_BuildValue("(ss)", (char *)type, (char *)fields);
  Py_RETURN_NONE;
}

//-------------------------------------------------------------------------
char *idc_guess_type(ea_t ea, char *buf, size_t bufsize)
{
  tinfo_t tif;
  if ( guess_tinfo2(ea, &tif) )
  {
    qstring out;
    if ( tif.print(&out) )
      return qstrncpy(buf, out.begin(), bufsize);
  }
  return NULL;
}

//-------------------------------------------------------------------------
char *idc_get_type(ea_t ea, char *buf, size_t bufsize)
{
  tinfo_t tif;
  if ( get_tinfo2(ea, &tif) )
  {
    qstring out;
    if ( tif.print(&out) )
    {
      qstrncpy(buf, out.c_str(), bufsize);
      return buf;
    }
  }
  return NULL;
}

//-------------------------------------------------------------------------
int idc_set_local_type(int ordinal, const char *dcl, int flags)
{
  if (dcl == NULL || dcl[0] == '\0')
  {
    if ( !del_numbered_type(idati, ordinal) )
        return 0;
  }
  else
  {
    tinfo_t tif;
    qstring name;
    if ( !parse_decl2(idati, dcl, &name, &tif, flags) )
      return 0;

    if ( ordinal <= 0 )
    {
      if ( !name.empty() )
        ordinal = get_type_ordinal(idati, name.begin());

      if ( ordinal <= 0 )
        ordinal = alloc_type_ordinal(idati);
    }

    if ( tif.set_numbered_type(idati, ordinal, 0, name.c_str()) != TERR_OK )
      return 0;
  }
  return ordinal;
}

//-------------------------------------------------------------------------
int idc_get_local_type(int ordinal, int flags, char *buf, size_t maxsize)
{
  tinfo_t tif;
  if ( !tif.get_numbered_type(idati, ordinal) )
  {
    buf[0] = 0;
    return false;
  }

  qstring res;
  const char *name = get_numbered_type_name(idati, ordinal);
  if ( !tif.print(&res, name, flags, 2, 40) )
  {
    buf[0] = 0;
    return false;
  }

  qstrncpy(buf, res.begin(), maxsize);
  return true;
}

//-------------------------------------------------------------------------
PyObject *idc_print_type(PyObject *py_type, PyObject *py_fields, const char *name, int flags)
{
  PYW_GIL_CHECK_LOCKED_SCOPE();
  if ( !PyString_Check(py_type) || !PyWStringOrNone_Check(py_fields) )
  {
    PyErr_SetString(PyExc_ValueError, "Typestring must be passed!");
    return NULL;
  }

  // To avoid release of 'type'/'fields' during Py_BEGIN|END_ALLOW_THREADS section.
  borref_t py_type_ref(py_type);
  borref_t py_fields_ref(py_fields);

  qstring res;
  const type_t *type   = (type_t *)PyString_AsString(py_type);
  const p_list *fields = PyW_Fields(py_fields);
  bool ok;
  Py_BEGIN_ALLOW_THREADS;
  tinfo_t tif;
  ok = tif.deserialize(idati, &type, &fields, NULL)
    && tif.print(&res, name, flags, 2, 40);
  Py_END_ALLOW_THREADS;
  if ( ok )
    return PyString_FromString(res.begin());
  else
    Py_RETURN_NONE;
}

//-------------------------------------------------------------------------
char idc_get_local_type_name(int ordinal, char *buf, size_t bufsize)
{
  const char *name = get_numbered_type_name(idati, ordinal);
  if ( name == NULL )
    return false;

  qstrncpy(buf, name, bufsize);
  return true;
}

//-------------------------------------------------------------------------
/*
#<pydoc>
def get_named_type(til, name, ntf_flags):
    """
    Get a type data by its name.
    @param til: the type library
    @param name: the type name
    @param ntf_flags: a combination of NTF_* constants
    @return:
        None on failure
        tuple(code, type_str, fields_str, cmt, field_cmts, sclass, value) on success
    """
    pass
#</pydoc>
*/
PyObject *py_get_named_type(const til_t *til, const char *name, int ntf_flags)
{
  const type_t *type = NULL;
  const p_list *fields = NULL, *field_cmts = NULL;
  const char *cmt = NULL;
  sclass_t sclass = sc_unk;
  uint32 value = 0;
  int code = get_named_type(til, name, ntf_flags, &type, &fields, &cmt, &field_cmts, &sclass, &value);
  if ( code == 0 )
    Py_RETURN_NONE;
  PyObject *tuple = PyTuple_New(7);
  int idx = 0;
#define ADD(Expr) PyTuple_SetItem(tuple, idx++, (Expr))
#define ADD_OR_NONE(Cond, Expr)                 \
  do                                            \
  {                                             \
    if ( Cond )                                 \
    {                                           \
      ADD(Expr);                                \
    }                                           \
    else                                        \
    {                                           \
      Py_INCREF(Py_None);                       \
      ADD(Py_None);                             \
    }                                           \
  } while ( false )

  ADD(PyInt_FromLong(long(code)));
  ADD(PyString_FromString((const char *) type));
  ADD_OR_NONE(fields != NULL, PyString_FromString((const char *) fields));
  ADD_OR_NONE(cmt != NULL, PyString_FromString(cmt));
  ADD_OR_NONE(field_cmts != NULL, PyString_FromString((const char *) field_cmts));
  ADD(PyInt_FromLong(long(sclass)));
  ADD(PyLong_FromUnsignedLong(long(value)));
#undef ADD_OR_NONE
#undef ADD
  return tuple;
}

//-------------------------------------------------------------------------
PyObject *py_get_named_type64(const til_t *til, const char *name, int ntf_flags)
{
  return py_get_named_type(til, name, ntf_flags | NTF_64BIT);
}

//-------------------------------------------------------------------------
int py_print_decls(text_sink_t &printer, til_t *til, PyObject *py_ordinals, uint32 flags)
{
  if ( !PyList_Check(py_ordinals) )
  {
    PyErr_SetString(PyExc_ValueError, "'ordinals' must be a list");
    return 0;
  }

  Py_ssize_t nords = PyList_Size(py_ordinals);
  ordvec_t ordinals;
  ordinals.reserve(size_t(nords));
  for ( Py_ssize_t i = 0; i < nords; ++i )
  {
    borref_t item(PyList_GetItem(py_ordinals, i));
    if ( item == NULL
      || (!PyInt_Check(item.o) && !PyLong_Check(item.o)) )
    {
      qstring msg;
      msg.sprnt("ordinals[%d] is not a valid value", int(i));
      PyErr_SetString(PyExc_ValueError, msg.begin());
      return 0;
    }
    uint32 ord = PyInt_Check(item.o) ? PyInt_AsLong(item.o) : PyLong_AsLong(item.o);
    ordinals.push_back(ord);
  }
  return print_decls(printer, til, ordinals.empty() ? NULL : &ordinals, flags);
}

//</inline(py_typeinf)>

//<code(py_typeinf)>
//-------------------------------------------------------------------------
// A set of tinfo_t & details objects that were created from IDAPython.
// This is necessary in order to clear all the "type details" that are
// associated, in the kernel, with the tinfo_t instances.
//
// Unfortunately the IDAPython plugin has to terminate _after_ the IDB is
// closed, but the "type details" must be cleared _before_ the IDB is closed.
static qvector<tinfo_t*> py_tinfo_t_vec;
static qvector<ptr_type_data_t*> py_ptr_type_data_t_vec;
static qvector<array_type_data_t*> py_array_type_data_t_vec;
static qvector<func_type_data_t*> py_func_type_data_t_vec;
static qvector<udt_type_data_t*> py_udt_type_data_t_vec;

static void __clear(tinfo_t *inst) { inst->clear(); }
static void __clear(ptr_type_data_t *inst) { inst->obj_type.clear(); inst->closure.clear(); }
static void __clear(array_type_data_t *inst) { inst->elem_type.clear(); }
static void __clear(func_type_data_t *inst) { inst->clear(); inst->rettype.clear(); }
static void __clear(udt_type_data_t *inst) { inst->clear(); }

void til_clear_python_tinfo_t_instances(void)
{
  // Pre-emptive strike: clear all the python-exposed tinfo_t
  // (& related types) instances: if that were not done here,
  // ~tinfo_t() calls happening as part of the python shutdown
  // process will try and clear() their details. ..but the kernel's
  // til-related functions will already have deleted those details
  // at that point.
  //
  // NOTE: Don't clear() the arrays of pointers. All the python-exposed
  // instances will be deleted through the python shutdown/ref-decrementing
  // process anyway (which will cause til_deregister_..() calls), and the
  // entries will be properly pulled out of the vector when that happens.
#define BATCH_CLEAR(Type)                                               \
  do                                                                    \
  {                                                                     \
    for ( size_t i = 0, n = py_##Type##_vec.size(); i < n; ++i )        \
      __clear(py_##Type##_vec[i]);                                      \
  } while ( false )

  BATCH_CLEAR(tinfo_t);
  BATCH_CLEAR(ptr_type_data_t);
  BATCH_CLEAR(array_type_data_t);
  BATCH_CLEAR(func_type_data_t);
  BATCH_CLEAR(udt_type_data_t);
#undef BATCH_CLEAR
}

#define DEF_REG_UNREG_REFCOUNTED(Type)                                  \
  void til_register_python_##Type##_instance(Type *inst)                \
  {                                                                     \
    /* Let's add_unique() it, because in the case of tinfo_t, every reference*/ \
    /* to an object's tinfo_t property will end up trying to register it. */ \
    py_##Type##_vec.add_unique(inst);                                   \
  }                                                                     \
                                                                        \
  void til_deregister_python_##Type##_instance(Type *inst)              \
  {                                                                     \
    qvector<Type*>::iterator found = py_##Type##_vec.find(inst);        \
    if ( found != py_##Type##_vec.end() )                               \
    {                                                                   \
      __clear(inst);                                                    \
      /* tif->clear();*/                                                \
      py_##Type##_vec.erase(found);                                     \
    }                                                                   \
  }

DEF_REG_UNREG_REFCOUNTED(tinfo_t);
DEF_REG_UNREG_REFCOUNTED(ptr_type_data_t);
DEF_REG_UNREG_REFCOUNTED(array_type_data_t);
DEF_REG_UNREG_REFCOUNTED(func_type_data_t);
DEF_REG_UNREG_REFCOUNTED(udt_type_data_t);

#undef DEF_REG_UNREG_REFCOUNTED

//</code(py_typeinf)>


#endif
