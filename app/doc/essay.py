#!/usr/bin/env python3
"""The swmm5-cplus essay: one source, rendered into the app's Doc tab and into Word.

Blocks: ('h2', title, anchor) ('p', html) ('fig', key, caption) ('code', lang, text)
('table', [header...], [[row...]...]) ('ul', [html...]) ('ol', [html...]) ('note', html)
Numbers in {braces} are filled from measured results by build.py.
"""

TITLE = "Same Numbers, Different Bones"
SUBTITLE = "How EPA SWMM 5.2.4 could have been built in C++, and a working engine that proves it"

BLOCKS = [
('h2', 'One model per customer', 'opening'),
('p', "Here is an experiment you can run in about a second. Take EPA SWMM 5.2.4, the engine most of the stormwater "
      "world runs on, and open a model. Then, without closing it, open a second one. The second "
      "<code>swmm_open()</code> returns 0, which in SWMM means success. Run the simulation and you get the second "
      "model's answers. The first model is simply gone. Its report file is empty, its binary output was never "
      "written, and AddressSanitizer counts {LEAK_BYTES} bytes of its memory in {LEAK_ALLOCS} allocations that "
      "nobody will ever free. A control run that opens only one model leaks nothing."),
('p', "Nothing crashed and nothing complained. The engine did exactly what it was built to do, because it was "
      "built for one model per process. Every array of nodes and links, every clock, every running total lives "
      "in a variable that belongs to the whole program. Open a second model and it writes over the first."),
('p', "That design was reasonable when SWMM 5 was first written, twenty-odd years ago. A simulation was a program you started, waited on, and read the "
      "report from. It is not reasonable now, when SWMM runs inside GUIs that hold several scenarios, inside "
      "calibration loops that want a thousand runs, inside Python, inside a browser tab, and next to other models "
      "it ought to be talking to. Bob Dickinson asked the obvious question: how could SWMM5 have been built in "
      "C++ so that it all still works? This essay answers it with an engine instead of an opinion."),

('h2', 'What swmm5-cplus is', 'what'),
('p', "swmm5-cplus is EPA SWMM 5.2.4 rebuilt as C++17 without changing one computed number. In the first step "
      "all {FUNCS} functions of the engine became members of a class called <code>Project</code>, and every one "
      "of the {MUTABLE} variables that SWMM writes while it runs became a field of that object. Later steps moved "
      "{FUNCS_OUT} general-purpose functions back out, the cross-section geometry, the expression parser, the "
      "integrator and the root finders, because they need no model at all; {FUNCS_NOW} remain members. Make two Projects and you have two "
      "independent engines in one program. Make a hundred. Step them in any order, from different threads, or "
      "inside one WebAssembly instance in a browser. The proof that nothing else changed is blunt: run the same "
      "input through EPA's C engine and through this one, and the binary <code>.out</code> files match byte for "
      "byte, as do the <code>.rpt</code> files once you drop the three lines that print the wall clock."),

('h2', 'Count before you cut', 'census'),
('p', "Anyone who starts tearing out structure before measuring it deserves what they get. So the "
      "first job was a census, done with <code>libclang</code>, which is the C compiler's own parser offered as a "
      "library. Regular expressions can guess at C; a compiler knows. It reads every one of the {CFILES} solver "
      "files ({CLINES} lines) and reports every variable declared outside a function, whether anything outside "
      "its file can see it, and every place it is used."),
('fig', 'state', "SWMM 5.2.4 holds {MUTABLE} mutable variables outside its functions. globals.h is only the "
                 "front door: {SHARED} more are defined inside modules and reached from other files, and {PRIVATE} "
                 "are used in one file only. All of them become per-Project fields."),
('p', "The front door is <code>globals.h</code>, with {GLOBALS} variables in plain sight: the node and link "
      "arrays, the error code, the clocks. That is the part everyone knows about. The census found two more "
      "rooms. In {EXTERN_FILES} files, a module reaches into another module's private variables by declaring them "
      "<code>extern</code> on the spot, bypassing every header. The mass balance totals, the statistics arrays and "
      "the output result vectors are wired together this way, {SHARED} variables in all. And {PRIVATE} more sit "
      "at the top of {PRIVATE_FILES} files and are used only there: {PRIV_STATIC} declared <code>static</code>, "
      "{PRIV_EXTERN} left with external linkage that nothing uses. Either way they are just as much "
      "one-per-process as the globals. The dynamic wave solver keeps its node work array there. So does the "
      "climate module, with 27 of them."),
('p', "Some things in that census are safe to share. Lookup tables that nothing ever writes, such as the "
      "keyword lists, can be shared by any number of models, and the census found {TABLES} of them plus "
      "{CONSTS} constants. The cross-section geometry tables are a special case. <code>xsect.dat</code> "
      "defines 49 tables and 43 sizes as plain globals, not even <code>const</code>; nothing writes them, so they "
      "stay shared, but only convention says so. A variable is only a problem if something writes to it. That "
      "distinction is the whole game."),

('h2', 'Let the compiler keep the books', 'lookup'),
('p', "Here is the trick that makes this tractable. In C, when a function in <code>dynwave.c</code> mentions "
      "<code>Node</code>, the compiler looks first among the function's own variables, then among the file's "
      "statics, then among the globals. In C++, when a member function of a class mentions <code>Node</code>, the "
      "compiler looks first among the function's own variables, then among the members of the object, then out "
      "in the namespace. Same order. Different owner."),
('fig', 'lookup', "An unqualified name resolves in the same order in both languages. Move the globals and "
                  "statics into the object and leave the read-only data outside, and every name in every function "
                  "body still means what it meant, without editing the body."),
('p', "So if every function becomes a member of <code>Project</code>, and every variable that used to be global "
      "becomes a member too, the bodies of 967 functions do not need to change. The compiler resolves each name "
      "exactly as before, except that it now finds the copy belonging to this simulation. The only bodies that do "
      "change are the ones whose file statics move into a private per-module structure, so that dynwave's "
      "<code>Xnode</code> becomes <code>dynwave_-&gt;Xnode</code>. That arrow is honest bookkeeping: it shows you, "
      "right in the code, which module owns which state."),
('p', "There is one way this can go wrong. If a name that used to mean something outside the class, such as a "
      "C library function or a constant, collides with a new member, the member wins and the meaning changes "
      "silently. The generator checks for exactly that before it writes a line, and it found five function names "
      "defined in more than one file (<code>getWidth</code> and <code>getArea</code> in both <code>dwflow.c</code> "
      "and <code>shape.c</code>, for instance). The duplicates got a module prefix. No other name collided."),

('h2', 'A refactor you can run again', 'generator'),
('p', "Nobody should make five thousand edits by hand and then claim they are all correct. So the lift is a "
      "program, <code>tools/make_cplus.py</code>, and the edits are its output. It parses EPA's source with "
      "<code>libclang</code>, and every change it makes sits at a location the compiler pointed to: a reference "
      "is renamed only where the compiler says it refers to that declaration. There is one place where text "
      "matching does the last inch: inside a macro argument the compiler reports the reference at the macro's "
      "name, so the generator finds the identifier in that argument's text by its spelling. Point it at the EPA 5.2.4 tag and it produces the same C++ tree every time."),
('fig', 'pipeline', "The refactor is a program. libclang finds every declaration and reference; the generator "
                    "makes {EDITS} edits; the compiler builds native and WebAssembly engines; the gate compares "
                    "the output bytes with EPA's, and any difference stops the line."),
('p', "The program made {EDITS} edits and rewrote {PRIV_REFS} references to private variables. {MACRO_REFS} "
      "references of all kinds were hiding inside macro arguments such as <code>FREE(x)</code>, <code>MIN(a,b)</code> "
      "and <code>UCF(LENGTH)</code>, where a naive tool loses track of them. Twenty-two places passed a function "
      "as a value, a callback to the root finder or the ODE solver or the expression parser; those needed the "
      "C++ spelling for a pointer to a member function. Five files had types that appear in function signatures, "
      "and those types moved into small headers. That is the entire list of complications. Then it compiled, "
      "and on the first smoke test of 40 EPA models every <code>.out</code> and <code>.rpt</code> matched."),
('p', "That first run did teach one lesson. Six models opened together and stepped in turn produced garbled "
      "reports. It looked like leftover shared state, which was the obvious suspect. AddressSanitizer said "
      "otherwise: a double free inside <code>swmm_close()</code>. EPA's close never clears its file pointers, so "
      "closing twice writes into a freed <code>FILE</code> and frees it again. EPA's own program never closes "
      "twice, so nobody noticed in twenty years. A host program that manages many models does close twice, and "
      "it noticed immediately. More on that below."),

('h2', 'Six steps, each paid for in bytes', 'ladder'),
('p', "The engine was rebuilt one gated step at a time, each a separate commit on top of EPA's pristine source, "
      "so the history itself is the record of what changed:"),
('table', ['Step', 'What it did', 'Source lines (+/-)', 'Gate'], [
    ['Stage 1', 'Mechanical lift: Project class, PIMPL module state, handle API', '+7,346 / -5,369', '40/40 smoke'],
    ['R1', 'Resource safety: idempotent close, bounded paths, no descriptor leak', '+124 / -7', '40/40; ASan clean'],
    ['R2', 'Root finder, ODE solver and expression parser as standalone components', '+459 / -548', '187/187 coverage'],
    ['R3', 'Cross-section geometry as shape objects, pure functions of the section', '+2,937 / -2,004', '187/187 coverage'],
    ['R3b', 'Shape table at namespace scope; C++ facade; coupling example', '+157 / -57', '{FULL_SAME}/{FULL_DONE} corpus'],
    ['R4', 'Outfall normal and critical depth reused when the flow repeats; profile-guided build', '+23 / -2', '217/221 smoke and coverage; 4 crash decks crash both alike'],
]),
('p', "The coverage set is 187 models chosen from Bob's corpus so that every feature the census could find "
      "is in at least one of them: all 26 cross-section shapes, every storage curve type, groundwater, "
      "treatment expressions, control rule expressions, LID, snow, RDII, inlets, culverts, pumps, and all three "
      "routing methods. Thirty-six of them are models EPA rejects with an error. Those count too: the error "
      "report has to match as well."),

('h2', 'Eight switches, turned sideways', 'shapes'),
('p', "If you want to see what C-style structure costs, look at cross sections. SWMM computes area from depth, "
      "depth from area, top width, hydraulic radius, section factor and its derivative with eight functions. Each "
      "one is a <code>switch</code> over 26 shapes. To add a shape you edit all eight, plus the parameter reader, "
      "plus two keyword lists, and you had better not miss one, because the compiler will not tell you."),
('p', "Lay those switches side by side and you have a table: 26 rows of shapes, 8 columns of quantities. "
      "Object-oriented code is the same table read the other way, one row per class. So "
      "<code>tools/transpose_xsect.py</code> transposes it. Each shape becomes a class. Each case body moves into "
      "that class word for word, with the function's opening lines repeated at the top so each method does "
      "exactly what the switch did, operation for operation. Each switch's <code>default</code> branch becomes "
      "the base class."),
('fig', 'shapes', "The 8 x 26 table of EPA's cross-section switches, transposed. A filled dot is code that was a "
                  "case for that shape and is now an override; an open dot fell to the default branch and now "
                  "inherits it."),
('p', "Three of the shapes needed one more change, and it is the change that matters most. Irregular transects, "
      "custom shapes and streets looked up their geometry tables in the global <code>Transect</code>, "
      "<code>Shape</code> and <code>Street</code> arrays on every single call. Now the table is bound into the "
      "cross section once, when the link is validated. After that, every geometry function in SWMM is a pure "
      "function of the cross section and nothing else. You can test it alone. You can call it from any thread. "
      "You can lift the file out and use it in another program, and it will not need a SWMM model to come along."),

('h2', 'Small tools should not know about the model', 'utilities'),
('p', "SWMM's root finder, its Runge-Kutta integrator and its formula parser are general numerical tools. In "
      "EPA's code each of them still depends on the program around it. The integrator kept its work arrays in "
      "seven global variables, one of them named <code>y</code>, visible to the linker across the whole program. "
      "It took the derivative routine as a plain C function pointer, so the derivative routine had to read its "
      "inputs from yet more globals. The parser kept its lexer state in ten file statics and its callback in an "
      "eleventh."),
('p', "In swmm5-cplus the root finders are templates that accept any callable. The caller passes a small "
      "lambda that captures the Project, and the compiler can inline it. The integrator is an "
      "<code>OdeSolver</code> object that owns its arrays and frees them in its destructor. Each formula gets its "
      "own <code>MathExprParser</code> object for as long as it takes to parse it. None of the three needs a "
      "Project to exist."),
('p', "Moving that code turned up one trap, and it is worth knowing about. EPA's <code>consts.h</code> defines "
      "<code>TINY</code> as 1.E-6. The integrator defines its own <code>TINY</code> as 1.0e-30. The C version "
      "works only because the integrator's source file never includes <code>consts.h</code>. Put both in one "
      "place and the preprocessor silently changes a constant in the integrator by twenty-four orders of "
      "magnitude. The C++ constants are named <code>kTiny</code> and friends now, which no header can touch."),

('h2', 'What a reentrant engine buys you', 'buys'),
('p', "Once all of a simulation's state lives in one object, things that were awkward or impossible become "
      "ordinary. Here are three, each measured in this build."),
('p', "<b>Many models in one process.</b> Six EPA models opened at once in a single WebAssembly instance and "
      "stepped one routing step each in turn, {MULTI_STEPS} steps in all, produced six <code>.out</code> files "
      "byte-identical to six separate EPA runs. The same test runs natively, under AddressSanitizer, and with "
      "one thread per model."),
('fig', 'multi', "Six models, one instance, round-robin stepping. Each model's output matches its own solo run of "
                 "EPA 5.2.4 byte for byte, so no state leaked from one to another."),
('p', "<b>Models that talk to each other.</b> The demo network is two models: an upstream catchment draining to "
      "outfall OA, and a downstream trunk with its own subcatchment. They run in lock step, 8,640 routing steps "
      "of five seconds each, and after each step the flow arriving at OA is handed to the downstream node IN as "
      "lateral inflow. The check is the same network built as one model, with the upstream pipe dropping five "
      "feet into IN where the outfall used to be."),
('fig', 'coupled', "One-way coupling at a free-drop interface. The coupled peak is {CPL_PEAK_C} cfs against "
                   "{CPL_PEAK_M} cfs for the merged model ({CPL_PEAK_PCT}); the five-minute volumes differ by "
                   "{CPL_VOL_PCT}. The difference is the interface: OA is a free outfall, the merged model has a "
                   "drop structure, and one-way coupling passes nothing back upstream."),
('p', "With EPA's engine this needs two processes and a pipe between them. With swmm5-cplus it is two objects "
      "and a function call. The answer is not identical to the merged model, and it should not be: a free "
      "outfall and a pipe dropping into a manhole are not the same hydraulics, and that two percent is the "
      "honest price of the simplification. What the demo shows is that the coupling itself adds nothing: the "
      "clocks never drift apart, and the third model, run for the check, sat in the same process the whole time."),
('p', "<b>A friendlier API.</b> The C interface gains a handle argument (<code>swmm5cp_open(h, ...)</code>), and "
      "a header-only C++ class, <code>swmm::Simulation</code>, closes and frees its project however it goes out "
      "of scope. EPA's original handle-less functions are still exported, each driving one default project per "
      "thread, so existing programs link without a change."),

('h2', 'How it was verified', 'verified'),
('p', "The parity contract is stated so that it can fail. Native swmm5-cplus, built with GCC 13 at "
      "<code>-O2</code> with link-time optimization, matches native EPA 5.2.4 built with the same compiler at "
      "plain <code>-O2</code>: the <code>.out</code> file byte for byte, and the <code>.rpt</code> "
      "file byte for byte after removing the three lines that print the date and elapsed wall time. "
      "WebAssembly swmm5-cplus matches EPA 5.2.4's C source compiled to WebAssembly with the same Clang 18 "
      "toolchain, under the same rule and the same difference in flags. The flags differ on purpose: the "
      "gate tests the C++ build that ships, and that build uses link-time optimization."),
('fig', 'gate', "The full corpus gate by folder: {FULL_SAME} models byte-identical, {FULL_CRASH} crash decks that "
                "crash both engines alike, {FULL_DIFF} different, and {FULL_INC} inconclusive because both engines hit "
                "the same time or file-size limit."),
('p', "The full gate runs every one of the {CORPUS_N} <code>.inp</code> files in Bob's 1729 corpus through both "
      "engines. {GATE_STATE}. Of those, {FULL_SAME} came out identical, {FULL_DIFF} "
      "different and {FULL_INC} inconclusive{GATE_REST}. Another {FULL_CRASH} are crash decks from the OpenSWMM code review, built to "
      "crash 5.2.4, and they crash both engines with the same signal and leave no output; they are counted "
      "apart, because a matching crash is bug-compatible, not correct. Of the identical runs, {EMPTY_OUT} stop "
      "on an input error and write no <code>.out</code>, so for those the report is the whole comparison. The WebAssembly gate ran the smoke and coverage "
      "sets through both WebAssembly engines: {WASM_SAME} of {WASM_N} identical."),
('p', "Two findings from that gate are about the platform, not the restructure. First, EPA's own C engine does "
      "not give the same bits natively and in WebAssembly: on {PLAT_DIFF} of the {WASM_N} WebAssembly-gated "
      "models the native and WebAssembly builds of the same C source differ, in the output values or in the "
      "order of tied entries in the report's ranking tables. Both WebAssembly engines agree with each other on "
      "every one of those models, so the cause sits in the platform. The math library differs between the two "
      "(glibc against wasi-libc), which is the likely reason; that has not been traced to a specific function. "
      "Second, glibc's fortify check aborts EPA's own <code>runswmm</code> whenever it is given a relative "
      "input path. That one is a real defect, and it is in the register below."),
('p', "Every comparison carries a control. The same engine run twice gives the same bytes. The six-model "
      "interleaving test is checked against six solo runs. The leak count for the two-open experiment is "
      "checked against a one-open run that leaks nothing."),
('p', "Time was measured, not assumed. Eight models of one to four seconds each were run five times, "
      "interleaved as EPA, C++, EPA again, so drift falls on both engines alike; the second EPA run measures the "
      "noise. At plain <code>-O2</code> the C++ build is {T_O2} times EPA's time, against a noise floor that "
      "wanders up to {T_O2_FLOOR}. Built with link-time optimization on both sides, the ratio is {T_LTO}, inside a "
      "floor of {T_LTO_FLOOR}: no measurable cost. That link-time optimization closes the gap points at calls that "
      "now cross files, the shape dispatch among them, though nobody has profiled it. And by the same standard, "
      "the six percent at plain <code>-O2</code> is only marginally resolved: on five of the eight models the "
      "five per-run ratios fall on both sides of 1.0."),
('fig', 'timing', "Run time, C++ over C, per model. Red dots: both at -O2. Green squares: both with link-time "
                  "optimization. Grey bars: how far EPA's own repeat run strayed from its first."),

('h2', 'What turned up along the way', 'defects'),
('p', "Rebuilding an engine to hold many models is a stress test of every place that assumed there would only "
      "be one. Ten findings went into the register. None of them changes a number SWMM computes, which is why "
      "they hid for so long. Here is each one as the code actually read in 5.2.4."),
('p', "<b>S5CP-1. <code>swmm_close()</code> leaves its file pointers dangling.</b> Close twice and the second "
      "close writes the run times into a freed <code>FILE</code> and frees it again. Fixed in R1: each pointer is "
      "cleared after it is closed."),
('code', 'c', """    report_writeSysTime();
    if ( Finp.file != NULL )
        fclose(Finp.file);
    if ( Frpt.file != NULL )
        fclose(Frpt.file);
    if ( Fout.file != NULL )
    {
        fclose(Fout.file);
        if ( Fout.mode == SCRATCH_FILE ) remove(Fout.name);
    }"""),
('p', "<b>S5CP-2. <code>getAbsolutePath()</code> can write past <code>InpDir</code>.</b> It is called as "
      "<code>getAbsolutePath(f1, InpDir, sizeof(InpDir))</code> with a 260-byte buffer. <code>realpath()</code> "
      "needs a buffer of <code>PATH_MAX</code>, which is 4,096 on Linux, and the absolute-path branch copies "
      "<code>strlen(fname)</code> bytes no matter what <code>size</code> says. glibc's fortify check sees the "
      "small buffer and aborts the program on every relative path, long or short. An earlier note in Bob's "
      "testing skill calls that abort a false positive; it is a real contract violation that overflows only "
      "for long paths. Fixed in R1."),
('code', 'c', """    // --- if fname has a relative path then retrieve its full path
    if (isRelativePath(fname))
    {
        #ifdef WINDOWS
            GetFullPathName((LPCSTR)fname, (DWORD)size, (LPSTR)absPath, NULL);
        #else
            realpath(fname, absPath);
        #endif
    }

    // --- otherwise copy fname to absPath
    else
    {
        sstrncpy(absPath, fname, strlen(fname));
    }"""),
('p', "<b>S5CP-3. Every scratch file leaks a descriptor.</b> On Linux and macOS, <code>mkstemp()</code> returns "
      "an open file descriptor and the code throws it away. The scratch files also land in the working directory, "
      "ignoring the TEMPDIR option. The descriptor leak is fixed in R1; the directory is not."),
('code', 'c', """    sstrncpy(fname, "swmmXXXXXX", MAXFNAME);
    mkstemp(fname);
    return fname;"""),
('p', "<b>S5CP-4. Opening a second project abandons the first.</b> <code>swmm_open()</code> resets the open flag "
      "instead of checking it, then opens the new project over the old one: success code, empty report, "
      "{LEAK_BYTES} bytes leaked. swmm5-cplus leaves the legacy API's behavior alone so it stays a drop-in "
      "replacement; the handle API simply never puts two projects in one object."),
('code', 'c', """        IsOpenFlag = FALSE;
        IsStartedFlag = FALSE;
        ExceptionCount = 0;

        // --- open a SWMM project
        strcpy(InpDir, "");
        project_open(f1, f2, f3);"""),
('p', "<b>S5CP-5 through S5CP-9</b> are fragilities more than defects. The two <code>TINY</code>s described "
      "above. <code>xsect.dat</code> gives its 92 variables external linkage, so exactly one file may include it. "
      "<code>infil.h</code> declares three globals, <code>HortInfil</code>, <code>GAInfil</code> and "
      "<code>CNInfil</code>, that no file defines. The integrator's work arrays are linker-visible globals, one of them named "
      "<code>y</code>. And twelve files reach into other modules through local <code>extern</code> "
      "declarations, which the census section described. R2 removed two of these. R3 made <code>xsect.dat</code> a single "
      "definition but left its linkage alone, and the dead declarations and the private wiring of the "
      "statistics module remain."),

('p', "<b>S5CP-10</b> turned up later, during the speed work, when link-time optimization compared every "
      "declaration with its definition. <code>report.c</code> and <code>error.c</code> disagree about the size "
      "of the same array:"),
('code', 'c', """// report.c, line 72
extern char   ErrString[81];           // defined in ERROR.C

// error.c, line 28
char  ErrString[256];"""),
('p', "It does no harm today, because the array is only ever handed on as a pointer, but C calls it undefined "
      "behavior, and the next compiler is free to agree. In swmm5-cplus the string is one member of "
      "<code>Project</code> with one type, so the question cannot arise."),

('h2', 'How fast can it go', 'speed'),
('p', "Once an engine can hold many models, the next question asks itself: how fast can one of them run? Bob "
      "asked it straight out. The answer starts with arithmetic every SWMM modeler already knows. Run time is the "
      "number of routing steps, times the iterations in each step, times the work done per element in each "
      "iteration. Code that must give EPA's answers bit for bit can only touch the last of those three. The "
      "first two belong to the model, its options and the numerical scheme."),
('p', "So the first job was to find out where the work goes. A sampling profiler ran eight corpus models picked "
      "to cover the ground: a large dynamic-wave network, a network converted from XP-SWMM, subcatchment "
      "runoff, a continuous simulation, an LID network, natural channels, a water-quality tracer, and a single "
      "short trapezoidal pipe."),
('fig', 'speed_profile', "Where EPA 5.2.4 spends its time, by function family. Cross-section geometry and the math "
                         "library take 25 to 72 percent of each run; the momentum and continuity equations "
                         "themselves are the smaller share."),
('p', "The physics is not where the time goes. Cross-section geometry and the math library take between a quarter "
      "and three quarters of every run. <code>pow()</code> alone takes 10 to 39 percent and is the busiest "
      "single function in four of the eight. That is not because <code>pow()</code> is slow. On this machine it "
      "costs about 14 nanoseconds, and <code>cbrt()</code> or <code>exp(log())</code> are no faster. It is "
      "because <code>pow()</code> sits inside Newton iterations that run millions of times. The worst offender "
      "is the outfall. On every iteration of every time step, SWMM recomputes the outfall conduit's normal "
      "depth, by Newton iteration, and its critical depth, whether or not the flow has changed. In the two-hour "
      "short-pipe model that comes to 4.8 million Newton solves."),
('p', "That pointed to the one change made to the engine in this round. Normal and critical depth depend on "
      "nothing but the conduit and its flow. If the flow is the same number as last time, the answer is the same "
      "number too, so the code keeps the last answer and hands it back:"),
('code', 'c', """q = fabs(Link[j].newFlow / Conduit[k].barrels);
if ( q == Link[j].memoQ )
{
    yNorm = Link[j].memoYnorm;     // same flow, same answer, same bits
    yCrit = Link[j].memoYcrit;
}
else
{
    yNorm = link_getYnorm(j, q);   // Newton: about 7 steps, each a pow()
    yCrit = link_getYcrit(j, q);
    Link[j].memoQ = q;
    Link[j].memoYnorm = yNorm;
    Link[j].memoYcrit = yCrit;
}"""),
('p', "In the water-quality model, 70 percent of those calls repeat a flow exactly. "
      "It runs a month in two-second steps, and for most of that month the flow at its two outfalls holds "
      "still. With the same patch in EPA's C, that model runs in {SP_MEMO_WQ} of its unpatched time; "
      "swmm5-cplus R4 runs it in {SP_R4_WQ} of the time EPA's default build takes. In the short-pipe model the flow changes on every call, nothing repeats, and "
      "nothing is saved. That is how an honest optimization behaves: it pays where the profile points and "
      "nowhere else."),
('fig', 'speed_identical', "CPU time against EPA 5.2.4 built with Ubuntu's default gcc -O2, for builds whose output "
                           "is byte-identical; the EPA C variants also have Ubuntu's hardening flags off. The grey band "
                           "is the noise, the reference build timed against itself. swmm5-cplus R4 with a profile-guided "
                           "build averages {SP_R4}."),
('p', "The compiler supplies the rest. Link-time optimization alone gives {SP_LTO}. Adding -O3 and "
      "profile-guided optimization gives {SP_PGOLTO}. On their own, -O3, turning off Ubuntu's hardening flags and "
      "-fno-math-errno are lost in the noise. Put the memo and the best build together and swmm5-cplus R4 takes "
      "{SP_R4} of EPA's time on the eight models, every output byte-identical. In the gate it matched on 217 of "
      "221 smoke and coverage models; the other four are crash decks that crash both engines alike. That is "
      "the ceiling for keeping EPA's numbers exactly: about a fifth."),
('p', "Threads look like the obvious next move, and EPA already has them: OpenMP loops over links and nodes in "
      "dynamic wave, switched on by the THREADS option. They give identical output. They are also slower. With "
      "two threads, {OMP_SLOWER} of {OMP_N} models took longer in wall time, up to {OMP_MAX} times as long, and "
      "the best case was {OMP_BEST}. Even with one thread, a build compiled with OpenMP runs {SP_OMP1} times as "
      "long as the reference build on average, and up to {SP_OMP1_MAX} times as long on the smallest models. The reason is plain once "
      "you look. The threads are gathered and released around each loop, on every iteration of every step, "
      "millions of times, for loops of a few hundred links. A thread pool that lives for the whole run, on "
      "networks of thousands of links, would change the arithmetic. This machine has two cores, and none of "
      "that was tested."),
('fig', 'speed_threads', "EPA's OpenMP with two threads against the same build run serially. Output is "
                         "identical; most models run slower, and where there is a gain it is {OMP_GAIN} percent or less."),
('p', "Everything past this point changes the answers, and the size of the change is the lesson. Fused "
      "multiply-add instructions give {SP_FMA} of the reference time and -ffast-math {SP_FAST}, both built with "
      "-O3, which alone is worth about three percent. Starting the outfall's Newton solve "
      "from its last answer, instead of from the middle of the section, takes {SP_WARM} of the memo build's time "
      "on average and {SP_WARM_SHORT} on the short-pipe model, and the new answer always lands inside the tolerance the old one "
      "was computed to. Even so, it moved the peak flow in one link of the LID network by 28 percent, and that "
      "model's continuity error from 0.24 to 0.38 percent. Fast-math moved a natural-channel link's peak by 11 "
      "percent. Dynamic wave in SWMM5 carries a small difference forward and sometimes grows it. A change in the "
      "last bit is a change to the model, and it has to be gated like one."),
('fig', 'speed_movement', "Speed bought against the largest change in any link's peak flow, one point per model "
                          "and change. Doubling the routing step buys nearly twice the speed, sometimes for almost "
                          "nothing and sometimes for 10 percent or more."),
('p', "The big lever is the number of steps. Doubling the maximum routing step ran 1.8 to 1.9 times faster on "
      "the three models where it set the step, which is the arithmetic above doing what it says. On the XP "
      "network and the natural channels the link peaks moved by 3.5 percent or less. On the 375-element network "
      "the outflow peak moved 12 percent, and that network was not converging to begin with: it averages 7.97 "
      "iterations of the 8 allowed. Skipping steady state ran a steady-flow model 7.75 times faster and lost 5 "
      "percent of its volume. Switching the 375-element network to the SLOT surcharge method drove its "
      "continuity error to -16,886 percent."),
('p', "So the fastest SWMM5 code, built the way this one was, comes in four pieces. First, an exact mode: "
      "swmm5-cplus R4 with a profile-guided build, which exists now. Second, geometry precomputed for every "
      "conduit, so that normal depth, critical depth and the friction term become table lookups instead of "
      "<code>pow()</code>, <code>sin()</code> and Newton. That goes after the quarter to three quarters of run "
      "time the profile found, and it changes the last bits, so it goes through the gate. Third, fewer steps: an "
      "implicit Newton scheme with a sparse solver, or local time steps, so that one short pipe stops setting "
      "the step for the whole network. Fourth, threads only for big networks, with a pool that outlives the "
      "iteration. The second, third and fourth were not built or timed here. The first was, and the "
      "measurements above say where the other three would pay."),

('h2', 'What this does not show', 'limits'),
('ul', [
    "It is not a new engine and it is not more accurate. Every number is EPA 5.2.4's, including EPA's own "
    "numerical defects; the three runoff defects registered during the runoff-solver work earlier this month "
    "(S5-RS1 to S5-RS3) are all still in it, by design.",
    "It fixes none of EPA's crashes. The {FULL_CRASH} crash decks in the corpus crash swmm5-cplus exactly as they "
    "crash EPA 5.2.4; only the resource defects in the register were fixed.",
    "It is only modestly faster. Keeping EPA's numbers exactly, the best build takes {SP_R4} of EPA's CPU time "
    "on eight models; every larger gain in the speed section changes the results. All timing is native, on one "
    "machine with two cores and 8 GB, on eight timing models plus two large ones; the profile is a sampling "
    "profile, accurate to a few percent. The engines inside this app are the earlier be57fec build, without "
    "the R4 memo.",
    "Parity is per platform. The contract is C against C++ on the same compiler family; it does not promise that "
    "a native run and a browser run give identical bits, and on {PLAT_DIFF} of {WASM_N} models they do not, for "
    "EPA's C code as much as for this one.",
    "{GATE_LIMIT}. The coverage set "
    "touched every shape and every feature the census could find, but a feature that appears in no corpus model "
    "has not been exercised. The models are Bob's corpus, not a random sample of the world's models.",
    "Thread safety was tested by running six models on six threads with identical results, not by an audit. The "
    "engine itself is still single-threaded per model; the OpenMP loops in dynamic wave are compiled out of "
    "every swmm5-cplus build. EPA's own OpenMP build was timed in the speed section, not audited.",
    "The design is a first floor, not the building. <code>Project</code> is still one large class, the modules "
    "still share the big arrays freely, link types are still a switch, and the LID solver still passes a pointer "
    "to a member function. The next section says what the rest would take.",
    "The two-model coupling is one-way and lagged by nothing more than the order of the two step calls; it is a "
    "demonstration of what the architecture allows, not a validated coupling method.",
]),

('h2', 'Where it goes from here', 'next'),
('p', "The obvious next move is to split <code>Project</code> into parts: a network, hydrology, hydraulics, "
      "water quality, input and output. Before anyone does that, look at what they would be splitting."),
('fig', 'heat', "Who touches what in EPA 5.2.4. The link and node arrays are read and written from two dozen "
                "modules each; {GLOBAL_MODULES} of the 54 modules reach into globals.h at all."),
('p', "The coupling is real and it is concentrated. Twenty-four modules reach into the link array and "
      "twenty-seven into the node array. That says the first parts to cut are not the modules but the data: give "
      "nodes and links a narrow interface that the routing, quality and reporting code each see only the slice "
      "of, then move the modules behind those interfaces one at a time, gating every step on the output bytes "
      "exactly as here. Links would then become polymorphic the same way the cross sections did, and the result "
      "writer would become one implementation of an output interface, with an in-memory results store as "
      "another, which is what a browser or a calibration loop actually wants."),
('p', "None of that has to happen in one jump, and none of it has to be taken on faith. The method is the "
      "product here as much as the code: measure the structure, let the compiler make the edits, and let the "
      "output bytes decide whether each step is kept. A refactor that cannot show its bytes is a rewrite with "
      "good intentions."),

('h2', 'How to use it', 'howto'),
('ol', [
    "<b>Try it here.</b> The Run tab runs any <code>.inp</code> through EPA 5.2.4 and swmm5-cplus side by side, "
    "both compiled to WebAssembly, and compares the outputs. Choose a sample or open your own file, plus any "
    "rainfall or other data files it names. Under the graph, a table compares the two series statistic by "
    "statistic: peak and its time, minimum, mean, volume, largest and mean absolute difference, RMSE, bias, "
    "Nash–Sutcliffe efficiency and R². Below that, every value in both <code>.out</code> files is compared, and "
    "then every number in both reports: headline results, the continuity and time-step tables, and each "
    "summary table column by column. Each table saves as CSV.",
    "<b>See the reentrancy.</b> Many at once opens all the samples in one swmm5-cplus instance and steps them in "
    "turn, then repeats EPA's two-open experiment so you can watch the first model vanish.",
    "<b>Couple two models.</b> The Coupled tab runs the upstream and downstream models in lock step and draws "
    "the result against the merged network, with the same statistics table, the merged network as the "
    "reference.",
    "<b>Build it.</b> Native: compile <code>src/engine/*.cpp</code> and <code>src/api/api.cpp</code> with "
    "<code>g++ -std=c++17 -O2 -flto</code>, or run <code>tools/build_pgo.sh</code> for the fastest exact "
    "build, and link EPA's own <code>main.c</code> for the <code>runswmm</code> command "
    "line. WebAssembly: <code>build_wasm.sh</code> with Clang 18 and wasi-libc.",
    "<b>Call it.</b> From C, <code>swmm5cp_create()</code> gives a handle and every <code>swmm_</code> function "
    "has a <code>swmm5cp_</code> twin that takes it. From C++, <code>swmm::Simulation</code>. Existing programs "
    "that call <code>swmm_run()</code> keep working unchanged.",
    "<b>Regenerate it.</b> <code>tools/make_cplus.py</code> rebuilds stage 1 from the EPA tag; "
    "<code>tools/transpose_xsect.py</code> rebuilds the shape classes; <code>tools/gate.py</code> reruns the "
    "byte comparison on any folder of models.",
]),

('h2', 'AI use', 'ai'),
('p', "swmm5-cplus, this app and this essay were made with Claude, Anthropic's AI model (Claude Opus 5.5), in a "
      "single Claude session on 27 and 28 October 2026, at the request of Robert (Bob) Dickinson, who asked the "
      "question and supplies the corpus the engine was tested on. Responsibility for publishing the work is his."),
('p', "<b>What Claude did.</b> It ran the libclang census, wrote the two generators (<code>make_cplus.py</code> "
      "and <code>transpose_xsect.py</code>) and every hand-written change after them, found and fixed the "
      "resource defects, wrote the C and C++ APIs, the coupling example, the WebAssembly builds and their "
      "in-memory file system, the test and timing harnesses, this app, the figures and this text. In a later "
      "round on 28 October it profiled the engine, ran the compiler, threading, option and arithmetic "
      "experiments, wrote the outfall memo that became R4 and the speed section above."),
('p', "<b>How it was checked.</b> The checks are mechanical, not the AI's judgment: byte comparison of "
      "<code>.out</code> and <code>.rpt</code> files against EPA SWMM 5.2.4 built from the official tag, "
      "AddressSanitizer runs, and interleaved timing with a noise floor. <b>What no person has checked yet:</b> "
      "Bob has not yet reviewed the generated C++ or the hand-written changes line by line, nor rerun the gate "
      "on his own machine."),
]
