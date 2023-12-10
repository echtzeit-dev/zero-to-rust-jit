use std::{env, ptr};
use std::ffi::{c_void, CStr, CString};
use std::io::Write;
use std::mem::transmute;
use std::process::exit;
use std::slice::from_raw_parts;
use console::Term;
use libc::{c_char, c_int};
use llvm_sys::bit_reader::LLVMParseBitcode2;
use llvm_sys::core::{LLVMContextSetDiagnosticHandler, LLVMCreateMemoryBufferWithContentsOfFile, LLVMDisposeMessage, LLVMGetDiagInfoDescription};
use llvm_sys::error::LLVMErrorRef;
use llvm_sys::orc2::lljit::{LLVMOrcCreateLLJIT, LLVMOrcLLJITAddLLVMIRModule, LLVMOrcLLJITGetMainJITDylib, LLVMOrcLLJITLookup, LLVMOrcLLJITRef};
use llvm_sys::orc2::{LLVMJITEvaluatedSymbol, LLVMJITSymbolFlags, LLVMOrcAbsoluteSymbols, LLVMOrcCLookupSet, LLVMOrcCreateCustomCAPIDefinitionGenerator, LLVMOrcCreateNewThreadSafeContext, LLVMOrcCreateNewThreadSafeModule, LLVMOrcCSymbolMapPair, LLVMOrcDefinitionGeneratorRef, LLVMOrcJITDylibAddGenerator, LLVMOrcJITDylibDefine, LLVMOrcJITDylibLookupFlags, LLVMOrcJITDylibRef, LLVMOrcJITTargetAddress, LLVMOrcLookupKind, LLVMOrcLookupStateRef, LLVMOrcMaterializationUnitRef, LLVMOrcRetainSymbolStringPoolEntry, LLVMOrcSymbolStringPoolEntryRef, LLVMOrcThreadSafeContextGetContext, LLVMOrcThreadSafeContextRef};
use llvm_sys::orc2::LLVMJITSymbolGenericFlags::LLVMJITSymbolGenericFlagsWeak;
use llvm_sys::prelude::{LLVMDiagnosticInfoRef, LLVMModuleRef};
use llvm_sys::support::LLVMParseCommandLineOptions;
use llvm_sys::target::{LLVM_InitializeNativeAsmPrinter, LLVM_InitializeNativeTarget};

fn hello_impl() {
    println!("Oh hello, that's called from JITed code!\n");
}

fn handle_undefined_symbol(mangled_name: &[u8]) -> LLVMOrcJITTargetAddress {
    if mangled_name == b"hello" {
        hello_impl as _
    } else {
        0
    }
}

unsafe fn add_module(jit: LLVMOrcLLJITRef, ctx: LLVMOrcThreadSafeContextRef, module: LLVMModuleRef) -> LLVMOrcJITDylibRef {
    let tsm = LLVMOrcCreateNewThreadSafeModule(module, ctx);
    let main_jd = LLVMOrcLLJITGetMainJITDylib(jit);

    let err = LLVMOrcLLJITAddLLVMIRModule(jit, main_jd, tsm);
    if err != ptr::null_mut() {
        panic!();
    }

    main_jd
}

extern "C" fn diagnostic_handler(di: LLVMDiagnosticInfoRef, _: *mut c_void) {
    unsafe {
        let cerr = LLVMGetDiagInfoDescription(di);
        eprintln!("Error in bitcode parser: {}", CStr::from_ptr(cerr).to_str().unwrap());
        LLVMDisposeMessage(cerr);
        exit(1);
    }
}

unsafe fn load_module(file_name: &str, ctx: LLVMOrcThreadSafeContextRef) -> LLVMModuleRef {
    let file_name = CString::new(file_name).unwrap();
    let mut mem_buf = ptr::null_mut();
    let mut err = ptr::null_mut();
    LLVMCreateMemoryBufferWithContentsOfFile(file_name.as_ptr(), &mut mem_buf as *mut _, &mut err as *mut _);
    assert_eq!(err, ptr::null_mut());

    let bare_ctx = LLVMOrcThreadSafeContextGetContext(ctx);
    LLVMContextSetDiagnosticHandler(bare_ctx, Some(diagnostic_handler), ptr::null_mut() as *mut _);

    let mut module = ptr::null_mut();
    let ec = LLVMParseBitcode2(mem_buf, &mut module as *mut _);
    assert_eq!(ec, 0);

    module
}

unsafe fn init() -> (String, LLVMOrcLLJITRef, LLVMOrcThreadSafeContextRef) {
    let mut args = env::args().collect::<Vec<_>>();
    if args.len() < 2 {
        panic!("Usage: %s sum.bc [ llvm-flags ]\n")
    }

    LLVMParseCommandLineOptions(
        args.len() as c_int,
        args.iter().map(|arg| arg.as_ptr() as *const c_char).collect::<Vec<_>>().as_ptr(),
        "".as_ptr() as *const c_char,
    );
    LLVM_InitializeNativeTarget();
    LLVM_InitializeNativeAsmPrinter();

    let mut jit = ptr::null_mut();
    let err = LLVMOrcCreateLLJIT(&mut jit as *mut _, ptr::null_mut());
    assert_eq!(err, ptr::null_mut());

    let ctx = LLVMOrcCreateNewThreadSafeContext();
    assert_ne!(ctx, ptr::null_mut());

    (args.swap_remove(1), jit, ctx)
}

fn main() {
    unsafe {
        let (file_name, jit, ctx) = init();

        let module = load_module(&file_name, ctx);
        let unit = add_module(jit, ctx, module);

        add_generator(unit, handle_undefined_symbol);

        let mut sum_fn_addr: LLVMOrcJITTargetAddress = 0;
        let name = CString::new("sum").unwrap();
        let err = LLVMOrcLLJITLookup(jit, &mut sum_fn_addr as *mut _, name.as_ptr() as *const c_char);
        assert_eq!(err, ptr::null_mut());

        let sum: extern "C" fn(i32, i32) -> i32 = transmute(sum_fn_addr);
        do_loop(sum);
    }
}

fn do_loop(sum: extern "C" fn(i32, i32) -> i32) {
    let mut term = Term::stdout();

    loop {
        write!(term, "a = ").unwrap();
        let a = term.read_line().unwrap().parse().unwrap();

        write!(term, "b = ").unwrap();
        let b = term.read_line().unwrap().parse().unwrap();

        write!(term, "\n").unwrap();

        write!(term, "\nsum({}, {}) = {}\n\n", a, b, sum(a, b)).unwrap();

        write!(term, "Again? (Y/n) ").unwrap();

        if term.read_char().unwrap() == 'n' {
            return;
        }

        write!(term, "\n\n").unwrap();
    }
}

fn drop_leading_underscores(mut name: &[u8]) -> &[u8] {
    while name[0] == b'_' {
        name = &name[1..];
    }
    name
}

extern "C" fn generator(
    _: LLVMOrcDefinitionGeneratorRef,
    ctx: *mut c_void,
    _: *mut LLVMOrcLookupStateRef,
    _: LLVMOrcLookupKind,
    jd: LLVMOrcJITDylibRef,
    _: LLVMOrcJITDylibLookupFlags,
    names: LLVMOrcCLookupSet,
    names_count: usize
) -> LLVMErrorRef {
    unsafe {
        let resolve: Resolve = transmute(ctx);
        for name in from_raw_parts(names, names_count) {
            let linker_mangled = CStr::from_ptr(name.Name as *const c_char);
            let mangled_name = drop_leading_underscores(linker_mangled.to_bytes());

            let handler = resolve(mangled_name);
            if handler == 0 {
                continue;
            }

            let mu = redirect(name.Name, handler);
            let err = LLVMOrcJITDylibDefine(jd, mu);
            assert_eq!(err, ptr::null_mut());
        }

        ptr::null_mut()
    }
}

unsafe fn redirect(name: LLVMOrcSymbolStringPoolEntryRef, addr: LLVMOrcJITTargetAddress) -> LLVMOrcMaterializationUnitRef {
    let flags = LLVMJITSymbolFlags {
        GenericFlags: LLVMJITSymbolGenericFlagsWeak as _,
        TargetFlags: 0,
    };
    let sym = LLVMJITEvaluatedSymbol {
        Address: addr,
        Flags: flags,
    };
    LLVMOrcRetainSymbolStringPoolEntry(name);
    let pair = LLVMOrcCSymbolMapPair {
        Name: name,
        Sym: sym,
    };

    let mut pairs = [pair];
    LLVMOrcAbsoluteSymbols(pairs.as_mut_ptr(), pairs.len())
}

type Resolve = fn(&[u8]) -> LLVMOrcJITTargetAddress;
unsafe fn add_generator(unit: LLVMOrcJITDylibRef, resolve: Resolve) {
    extern "C" fn dispose(_: *mut c_void) {}
    let generator = LLVMOrcCreateCustomCAPIDefinitionGenerator(generator, resolve as *mut c_void, dispose);
    LLVMOrcJITDylibAddGenerator(unit, generator)
}