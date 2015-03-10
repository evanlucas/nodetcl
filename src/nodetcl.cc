/*
 * NodeTcl is a native Node extension that embeds a Tcl interpreter
 * within the Node.JS environment, allowing you to invoke Tcl commands
 * from within JavaScript code.
 *
 * Copyright (c) 2011, FlightAware LLC
 * All rights reserved.
 * Distributed under BSD license.  See LICENSE for details.
 */

#include <v8.h>
#include <node.h>
#include <tcl.h>
#include <string.h>
#include <stdlib.h>
#include <nan.h>

using namespace node;
using namespace v8;
static Persistent<FunctionTemplate> s_ct;

class NodeTcl : public ObjectWrap
{
private:
  Tcl_Interp *m_interp;
  int time_limit;
public:

  static void Init(Handle<Object> target)
  {
    NanScope();

    Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName(NanNew("NodeTcl"));

    NODE_SET_PROTOTYPE_METHOD(t, "eval", NodeTcl::Eval);
    NODE_SET_PROTOTYPE_METHOD(t, "proc", NodeTcl::Proc);
    NODE_SET_PROTOTYPE_METHOD(t, "call", NodeTcl::Call);
    NODE_SET_PROTOTYPE_METHOD(t, "getStacktrace", NodeTcl::LastError);
    NODE_SET_PROTOTYPE_METHOD(t, "setTimeLimit", NodeTcl::SetTimeLimit);
    NODE_SET_PROTOTYPE_METHOD(t, "getTimeLimit", NodeTcl::GetTimeLimit);
    NODE_SET_PROTOTYPE_METHOD(t, "makeSafe", NodeTcl::MakeSafe);
    NODE_SET_PROTOTYPE_METHOD(t, "deleteProc", NodeTcl::DeleteCommand);
    NODE_SET_PROTOTYPE_METHOD(t, "process_events", NodeTcl::Event);
    NanAssignPersistent(s_ct, t);
    target->Set(NanNew("NodeTcl"), t->GetFunction());
  }

  NodeTcl()
  {
    m_interp = Tcl_CreateInterp();
    time_limit = 0;
  }

  ~NodeTcl()
  {
    Tcl_DeleteInterp(m_interp);
    m_interp = NULL;
    NanDisposePersistent(s_ct);
  }

  // ----------------------------------------------------

  /*
   * new NodeTcl() -- allocate and return a new interpreter.
   */

  static NAN_METHOD(New) {
    NanScope();
    NodeTcl* hw = new NodeTcl();
    if (Tcl_Init(hw->m_interp) == TCL_ERROR) {
      Local<String> err = NanNew<String>(Tcl_GetStringResult(hw->m_interp));
      delete hw;
      NanThrowError(err);
    }
    hw->Wrap(args.This());
    NanReturnValue(args.This());
  }

  // ----------------------------------------------------

  /*
   * MakeSafe() -- transforms interpreter into a safe interpreter
   */
  static NAN_METHOD(MakeSafe) {
    NanScope();
    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    int ret = Tcl_MakeSafe(hw->m_interp);

    Local<Integer> result = NanNew<Integer>(ret);
    NanReturnValue(result);
  }

  // ----------------------------------------------------

  /*
   * DeleteCommand(String) -- remove a command from the interpreter
   */
  static NAN_METHOD(DeleteCommand)
  {
    NanScope();
    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    if (args.Length() != 1 || !args[0]->IsString())
      NanThrowTypeError("Argument must be a string");

    String::Utf8Value str(args[0]);
    const char* cstr = *str;

    int ret = Tcl_DeleteCommand(hw->m_interp, cstr);
    ret++;

    Local<Integer> result = NanNew<Integer>(ret);
    NanReturnValue(result);
  }

  // ----------------------------------------------------

  /*
   * LastError() -- gets stacktrace for the last error that occurred
   */
  static NAN_METHOD(LastError)
  {
    NanScope();
    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    Tcl_Obj *options = Tcl_GetReturnOptions(hw->m_interp, TCL_ERROR);
    Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
    Tcl_Obj *stacktrace;

    Tcl_IncrRefCount(key);
    Tcl_IncrRefCount(options);
    Tcl_DictObjGet(NULL, options, key, &stacktrace);
    Tcl_DecrRefCount(options);
    Tcl_DecrRefCount(key);

    Local<String> result = NanNew<String>(Tcl_GetStringFromObj(stacktrace, NULL));
    NanReturnValue(result);
  }

  // ----------------------------------------------------

  /*
   * SetTimeLimit(Integer) -- sets time limit for next Tcl command
   */
  static NAN_METHOD(SetTimeLimit) {
    NanScope();
    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    if (args.Length() != 1 || ! args[0]->IsUint32())
      NanThrowTypeError("Argument must be an integer");

    int limit = args[0]->ToInteger()->Value();
    hw->time_limit = limit;
    NanReturnValue(args[0]);
  }

  // ----------------------------------------------------

  static NAN_METHOD(GetTimeLimit) {
    NanScope();
    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    Local<Integer> result = NanNew<Integer>(hw->time_limit);
    NanReturnValue(result);
  }

  // ----------------------------------------------------

  /*
   * converts JavaScript objects into Tcl objects (can handle arrays, strings,
   * numbers, booleans and simple key-value-mapping objects -- might die
   * horribly if fed with the wrong data types)
   */
  static Tcl_Obj* jsToTcl(Local<Value> var, Tcl_Interp* interp) {
    Tcl_Obj* result;


    if (var->IsArray()) {
      result = Tcl_NewListObj(0, NULL);
      Local<Array> arrvar = Local<Array>::Cast(var);

      for (unsigned int i = 0; i < arrvar->Length(); i++) {
        Tcl_ListObjAppendElement(
          interp,
          result,
          jsToTcl(arrvar->Get(i), interp)
        );
      }


    } else if (var->IsBoolean()) {
      if (var->IsTrue())
        result = Tcl_NewBooleanObj(1);
      else
        result = Tcl_NewBooleanObj(0);


    } else if (var->IsObject()) {
      result = Tcl_NewDictObj();
      Local<Object> objvar = Local<Object>::Cast(var);
      Local<Array> keys = objvar->GetPropertyNames();

      for (unsigned int i = 0; i < keys->Length(); i++) {
        Tcl_DictObjPut(
          interp,
          result,
          jsToTcl(keys->Get(i), interp),
          jsToTcl(objvar->Get(keys->Get(i)), interp)
        );
      }


    } else {
      String::Utf8Value str(var);
      const char* cstr = *str;
      result = Tcl_NewStringObj(cstr, strlen(cstr));
    }

    return result;
  }

  // ----------------------------------------------------

  /*
   * converts a Tcl object into the corresponding JavaScript types
   */
  static Local<Value> tclToJs(Tcl_Obj *obj, Tcl_Interp *interp) {
    if (obj->typePtr && ! strcmp(obj->typePtr->name, "dict")) {
      Tcl_DictSearch search;
      Tcl_Obj *key, *value;
      int done;

      Tcl_DictObjFirst(interp, obj, &search, &key, &value, &done);
      Local<Object> result = NanNew<Object>();

      for (; !done; Tcl_DictObjNext(&search, &key, &value, &done)) {
        Handle<Value> x = NanNew<String>(Tcl_GetString(key));
        Handle<Value> y = tclToJs(value, interp);
        result->Set(x, y);
      }

      Tcl_DictObjDone(&search);

      return result;


    } else if (obj->typePtr && ! strcmp(obj->typePtr->name, "list")) {
      int objc;
      Tcl_Obj **objv;

      Tcl_ListObjGetElements(interp, obj, &objc, &objv);
      Local<Array> result = NanNew<Array>(objc);

      for (int i = 0; i < objc; i++) {
        Handle<Number> x = NanNew<Number>(i);
        Handle<Value>  y = tclToJs(objv[i], interp);
        result->Set(x, y);
      }

      return result;


    } else if (obj->typePtr && ! strcmp(obj->typePtr->name, "int")) {
      long john;
      Tcl_GetLongFromObj(interp, obj, &john);

      return NanNew<Number>(john);


    } else if (obj->typePtr && ! strcmp(obj->typePtr->name, "double")) {
      double cheese;
      Tcl_GetDoubleFromObj(interp, obj, &cheese);

      return NanNew<Number>(cheese);


    } else {
      return NanNew<String>(Tcl_GetUnicode(obj));
    }
  }

  // ----------------------------------------------------

  /*
   * call(String, ...) -- calls a Tcl proc with some arguments
   */
  static NAN_METHOD(Call) {
    NanScope();

    if (args.Length() < 1 || !args[0]->IsString())
      NanThrowTypeError("Argument must be a string");

    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());
    Tcl_Obj** params = (Tcl_Obj**)malloc(args.Length() * sizeof(Tcl_Obj*));
    int i;

    for (i = 0; i < args.Length(); i++) {
      Tcl_Obj* obj = jsToTcl(args[i], hw->m_interp);
      Tcl_IncrRefCount(obj);
      params[i] = obj;
    }

    if (hw->time_limit) {
      Tcl_Time limit;
      Tcl_GetTime(&limit);

      limit.sec += hw->time_limit;

      Tcl_LimitTypeSet(hw->m_interp, TCL_LIMIT_TIME);
      Tcl_LimitSetTime(hw->m_interp, &limit);
    } else {
      Tcl_LimitTypeReset(hw->m_interp, TCL_LIMIT_TIME);
    }

    int cc = Tcl_EvalObjv(hw->m_interp, args.Length(), params, 0);
    if (cc != TCL_OK) {
      for (i = 0; i < args.Length(); i++)
        Tcl_DecrRefCount(params[i]);
      free(params);
      Tcl_Obj *obj = Tcl_GetObjResult(hw->m_interp);
      return NanThrowError(Tcl_GetString(obj));
    }

    Tcl_Obj *obj = Tcl_GetObjResult(hw->m_interp);

    for (i = 0; i < args.Length(); i++)
      Tcl_DecrRefCount(params[i]);
    free(params);

    NanReturnValue(tclToJs(obj, hw->m_interp));
  }

  // ----------------------------------------------------

  /*
   * eval(String) -- execute some Tcl code and return the result.
   */
  static NAN_METHOD(Eval) {
    NanScope();

    if (args.Length() != 1 || !args[0]->IsString()) {
      return NanThrowTypeError("Argument must be a string");
    }

    Local<String> script = Local<String>::Cast(args[0]);

    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    if (hw->time_limit) {
      Tcl_Time limit;
      Tcl_GetTime(&limit);

      limit.sec += hw->time_limit;

      Tcl_LimitTypeSet(hw->m_interp, TCL_LIMIT_TIME);
      Tcl_LimitSetTime(hw->m_interp, &limit);
    } else {
      Tcl_LimitTypeReset(hw->m_interp, TCL_LIMIT_TIME);
    }

    const char* v = *String::Utf8Value(script);
    int cc = Tcl_EvalEx(hw->m_interp, v, -1, 0);
    if (cc != TCL_OK) {
      Tcl_Obj *obj = Tcl_GetObjResult(hw->m_interp);
      return NanThrowError(Tcl_GetString(obj));
    }

    Tcl_Obj *obj = Tcl_GetObjResult(hw->m_interp);

    NanReturnValue(hw->tclToJs(obj, hw->m_interp));
  }

  // ----------------------------------------------------

  /*
   * Data about a JavaScript function that can be executed.
   */
  struct callback_data_t {
    NodeTcl *hw;
    Tcl_Command cmd;
    NanCallback *jsfunc;
  };


  /*
   * Called by Tcl when it wants to invoke a JavaScript function.
   */
  static int CallbackTrampoline (ClientData clientData,
           Tcl_Interp *interp,
           int objc,
           Tcl_Obj* const* objv)
  {
    callback_data_t *cbdata = static_cast<callback_data_t*>(clientData);

    // Convert all of the arguments, but skip the 0th element (the procname).

    // This is horrible but the only way I've been able to
    // get clang to compile and keep from aborting due to overflow
    Handle<Value> jsArgv[5];

    for (int i = 1; i < objc; i++) {
      jsArgv[i - 1] = tclToJs((Tcl_Obj*)objv[i], interp);
    }

    // Execute the JavaScript method.
    TryCatch try_catch;
    Local<Value> result = cbdata->jsfunc->Call(objc - 1, jsArgv);

    // If a JavaScript exception occurred, send back a Tcl error.
    if (try_catch.HasCaught()) {
      Local<Message> msg = try_catch.Message();

      // pass back the exception message.
      v8::String::Utf8Value msgtext(msg->Get());
      Tcl_SetObjResult(interp, Tcl_NewStringObj(*msgtext, -1));

      // pass back the Return Options with the stack details.
      v8::String::Utf8Value filename(msg->GetScriptResourceName());
      v8::String::Utf8Value sourceline(msg->GetSourceLine());
      int linenum = msg->GetLineNumber();

      Tcl_SetErrorCode(interp, *sourceline, NULL);
      Tcl_AddErrorInfo(interp, "\n   occurred in file ");
      Tcl_AddErrorInfo(interp, *filename);
      Tcl_AddErrorInfo(interp, " line ");
      Tcl_AppendObjToErrorInfo(interp, Tcl_NewIntObj(linenum));


      // append the full stack trace if it was present.
      v8::String::Utf8Value stack_trace(try_catch.StackTrace());
      if (stack_trace.length() > 0) {
        Tcl_AddErrorInfo(interp, *stack_trace);
      }

      return TCL_ERROR;
    }

    // Pass back to Tcl the returned value
    Tcl_SetObjResult(interp, jsToTcl(result, interp));
    return TCL_OK;
  }


  /*
   * Called by Tcl when it wants to delete the proc.
   */
  static void CallbackDelete (ClientData clientData)
  {
    callback_data_t *cbdata = static_cast<callback_data_t*>(clientData);

    cbdata->hw->Unref();
    delete cbdata;
  }

  /*
   * proc(String, Function) -- define a proc within the Tcl namespace that executes a JavaScript function callback.
   */
  static NAN_METHOD(Proc)
  {
    NanScope();

    if (args.Length() != 2) {
      NanThrowTypeError("Expecting 2 arguments (String, Function)");
    }
    if (!args[0]->IsString()) {
      NanThrowTypeError("Argument 1 must be a string");
    }
    Local<String> cmdName = Local<String>::Cast(args[0]);
    if (!args[1]->IsFunction()) {
      NanThrowTypeError("Argument 2 must be a function");
    }

    Local<Function> jsfunc = Local<Function>::Cast(args[1]);

    NodeTcl* hw = ObjectWrap::Unwrap<NodeTcl>(args.This());

    callback_data_t *cbdata = new callback_data_t();
    cbdata->hw = hw;
    cbdata->jsfunc = new NanCallback(jsfunc);
    cbdata->cmd = Tcl_CreateObjCommand(hw->m_interp, (const char*)*String::Utf8Value(cmdName), CallbackTrampoline, (ClientData) cbdata, CallbackDelete);

    hw->Ref();

    NanReturnUndefined();
  }

  // ----------------------------------------------------

  /*
   * event(Boolean) -- handle one or all pending Tcl events if boolean is present and true
   */
  static NAN_METHOD(Event)
  {
    NanScope();
    int eventStatus;
    int doMultiple;

    if (args.Length() > 1 || (args.Length() == 1 && !args[0]->IsBoolean())) {
      NanThrowTypeError("Optional argument, if present, must be a boolean");
    }

    if (args.Length() == 0)  {
      doMultiple = 1;
    } else {
      doMultiple = args[0]->ToBoolean()->Value();
    }

    do {
      eventStatus = Tcl_DoOneEvent (TCL_ALL_EVENTS | TCL_DONT_WAIT);
    } while (doMultiple && eventStatus);

    Local<Integer> result = NanNew<Integer>(eventStatus);
    NanReturnValue(result);
  }

  // ----------------------------------------------------

};

extern "C" {
  static void init (Handle<Object> target)
  {
    NodeTcl::Init(target);
  }

  NODE_MODULE(nodetcl, init)
};
