// libffitest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include "atlsafe.h"
#include "libffitest_h.h"
#include "libffitest_i.c"
#include <dispex.h>

#include "libffi/ffi.h"

#import "msscript.ocx" raw_interfaces_only // msscript.ocx 
using namespace MSScriptControl;

struct LibFFIAtlModule : CAtlModule {
    HRESULT AddCommonRGSReplacements(IRegistrarBase *) {
        return S_OK;
    }
} _atlmodule;

struct FFIFunction : CComObjectRoot, IDispatch
{    
    BEGIN_COM_MAP(FFIFunction)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo)
    {        
        return S_FALSE;
    }

    STDMETHODIMP GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
    {
        return S_FALSE;
    }

    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
    {
        return S_FALSE;
    }

    STDMETHODIMP Invoke(DISPID id, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdp, VARIANT* pvarRes, EXCEPINFO* pei, UINT* puArgErr)
    {
        char params[4096]; // up to 4096 bytes parameters
        int index=0;
        for(int i=0;i<pdp->cArgs;i++)
        {
            VARIANT &v=pdp->rgvarg[i];
            switch(v.vt)
            {
            case VT_I4:
                *((int**)(params+index))=&v.intVal;
                index+=4;
                break;
            case VT_BSTR:
                *((wchar_t***)(params+index))=&v.bstrVal;
                index+=4;
                break;
            }
        }

        // we need to convert params to acceptable input
        char rval[4096];
        ffi_call(m_cif,(void (*)(void))m_farproc,(void*)rval,(void**)params);
        return S_OK;
    }
    static FFIFunction *Make(FARPROC farproc, ffi_cif *cif)
    {
        CComObject<FFIFunction> *p;
        CComObject<FFIFunction>::CreateInstance(&p);
        p->m_cif=cif;
        p->m_farproc=farproc;
        p->AddRef();
        return p;
    }
    FARPROC  m_farproc;
    ffi_cif *m_cif;
};

struct FFIObject : CComObjectRoot,  
                    IDispatchImpl<ILibFFITest, &__uuidof(ILibFFITest), &LIBID_FFILib, 0xFFFF, 0xFFFF>
{    
    BEGIN_COM_MAP(FFIObject)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    ffi_type *getFFITypeOfId(int i)
    {
        switch(i) {
            case 0: return &ffi_type_void; break;
            case 1: return &ffi_type_sint32; break;
            case 2: return &ffi_type_float; break;
            case 3: return &ffi_type_double; break;
            case 4: return &ffi_type_longdouble; break;
            case 5: return &ffi_type_uint8; break;
            case 6: return &ffi_type_sint8; break;
            case 7: return &ffi_type_uint32; break;
            case 8: return &ffi_type_sint16; break;
            case 9: return &ffi_type_uint32; break;
            case 10: return &ffi_type_sint32; break;
            case 11: return &ffi_type_uint64; break;
            case 12: return &ffi_type_sint64; break;
            case 13: break;                                 // each structure type should have its own id and return a structure type of correct length
            case 14: return &ffi_type_pointer; break;
            //case 15: return &ffi_type_pointer; break;   // char*
            //case 16: return &ffi_type_pointer; break;   // wchar*
        }
        return 0;
    }

    STDMETHOD(newCFunction)(BSTR absFnName_, INT retValType_, SAFEARRAY **argsTypes_, VARIANT *pCFunc_)
    {
        CComSafeArray<VARIANT> argsTypes__(*argsTypes_);
        ffi_cif  *cif=new ffi_cif;
        ffi_type *retValType = getFFITypeOfId(retValType_);
        ffi_type **argsTypes = (ffi_type**)::malloc(sizeof(ffi_type*)*(*argsTypes_)->cbElements);
        for(int i=0; i<argsTypes__.GetCount(); i++) {
            UINT vt=argsTypes__.GetAt(i).vt;
            if(vt!=VT_I4)
                return S_FALSE;
            argsTypes[i]=getFFITypeOfId(argsTypes__.GetAt(i).intVal);
        }
        ffi_prep_cif(cif, FFI_DEFAULT_ABI, argsTypes__.GetCount(), retValType, argsTypes);

        CString absFnName(absFnName_);
        int separatorPos=absFnName.Find(_T("!"));
        if(separatorPos==-1)
            return S_FALSE;
        
        CString libName(absFnName.Left(separatorPos));
        CString fnName(absFnName.Mid(separatorPos+1));
        HMODULE hmod=LoadLibrary(libName);
        if(!hmod)
            return S_FALSE;

        FARPROC farproc;        
        farproc=GetProcAddress(hmod, CStringA(fnName));
        

        pCFunc_->pdispVal=FFIFunction::Make(farproc,cif);
        pCFunc_->vt=VT_DISPATCH;
        
        return S_OK;
    }
    STDMETHOD(malloc)(INT s, INT *ptr)
    {
        *ptr=(INT)::malloc(s);
        return S_OK;
    }
    STDMETHOD(free)(INT ptr)
    {
        ::free((void*)ptr);
        return S_OK;
    }
};


int _tmain(int argc, _TCHAR* argv[])
{
    CoInitialize(0);
    HRESULT hr;
    LibFFIAtlModule::m_libid=LIBID_FFILib;
    
    IScriptControlPtr pScriptControl;
    pScriptControl.CreateInstance(__uuidof(ScriptControl));    
	pScriptControl->put_AllowUI(VARIANT_TRUE);
    pScriptControl->put_Timeout(-1);
    VARIANT_BOOL noSafeSubset(VARIANT_FALSE);
    pScriptControl->get_UseSafeSubset(&noSafeSubset);
    pScriptControl->put_Language(_bstr_t(L"JScript"));

    CComObject<FFIObject> *pFFIObject=0;
    CComObject<FFIObject>::CreateInstance(&pFFIObject);
    pFFIObject->AddRef();

    hr=pScriptControl->AddObject(_bstr_t("LIBFFI"), pFFIObject, VARIANT_TRUE);

    _variant_t v;
    hr=pScriptControl->AddCode(_bstr_t(L"VOID=0;"
                                       L"HWND=BOOL=INT32=1;"
                                       L"FLOAT=2;\n"
                                       L"DOUBLE=3;\n"
                                       L"LONGDOUBLE=4;\n"
                                       L"UINT8=5;\n"
                                       L"INT8=6;\n"
                                       L"UINT32=9;\n"
                                       L"INT32=10;\n"
                                       L"UINT64=11;\n"
                                       L"INT64=12;\n"
                                       L"STRUCT=13;\n"
                                       L"LPCHAR=LPWCHAR=POINTER=14;\n"));
    
    hr=pScriptControl->Eval(_bstr_t("messagebox=newCFunction('User32.dll!MessageBoxW', HWND, BOOL, LPWCHAR, LPWCHAR, UINT32);\n"
                                    "messagebox(0,'hello','hello',0);"), &v);

    hr=pScriptControl->Eval(_bstr_t("memptr=malloc(30);\n"
                                    "free(memptr)"), &v);
    
    pScriptControl=0;
    CoUninitialize();

	return 0;
}

/*
struct Test {
    int i;
    int ii;
    int iii;
};

unsigned char
foo(unsigned int x, float y, char *str, Test t)
{
    unsigned char result = x - y;
    return result;
}

int _tmain(int argc, _TCHAR* argv[])
{
    ffi_cif cif;
    ffi_type *arg_types[4];
    void *arg_values[4];
    ffi_status status;

    // Because the return value from foo() is smaller than sizeof(long), it
    // must be passed as ffi_arg or ffi_sarg.
    ffi_arg result;

    // Specify the data type of each argument. Available types are defined
    // in <ffi/ffi.h>.
    arg_types[0] = &ffi_type_uint;
    arg_types[1] = &ffi_type_float;
    arg_types[2] = &ffi_type_pointer;

    ffi_type ffi_type_test;
    ffi_type_test.size = sizeof(Test);
    ffi_type_test.alignment = sizeof(Test);
    ffi_type_test.type = FFI_TYPE_STRUCT;
    ffi_type *ffi_type_test_elements[3];
    ffi_type_test_elements[0]=&ffi_type_uint;
    ffi_type_test_elements[1]=&ffi_type_uint;
    ffi_type_test_elements[2]=&ffi_type_uint;

    ffi_type_test.elements = ffi_type_test_elements;

    arg_types[3] = &ffi_type_test;


    //arg_types[3].

    //FFI_TYPE_STRUCT

    // Prepare the ffi_cif structure.
    if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, &ffi_type_uint8, arg_types)) != FFI_OK)
    {
    // Handle the ffi_status error.
    }

    // Specify the values of each argument.
    unsigned int arg1 = 42;
    float arg2 = 5.1;
    char *ptr=0;
    Test t;
    t.i=0;
    t.ii=1;
    t.iii=2;

    arg_values[0] = &arg1;
    arg_values[1] = &arg2;
    arg_values[2] = &ptr;
    arg_values[3] = &t;

    // Invoke the function.
    ffi_call(&cif, FFI_FN(foo), &result, arg_values);

    // The ffi_arg 'result' now contains the unsigned char returned from foo(),
    // which can be accessed by a typecast.
    printf("result is %hhu", (unsigned char)result);
}*/