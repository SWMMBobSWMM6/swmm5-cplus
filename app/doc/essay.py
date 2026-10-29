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

('h2', 'The other C++ SWMM', 'swmm6'),
('p', "swmm5-cplus is not the only SWMM written in C++. SWMM6, the OpenSWMM engine from HydroCouple with Caleb "
      "Buahin as lead developer, is C++20, and it took the opposite road. swmm5-cplus moved EPA's code into "
      "objects and left every line of arithmetic alone. SWMM6 was written fresh, with new data structures, new "
      "solvers, threads and a large public API. Where its answers must match the old engine, the new code is "
      "bent back until they do, and 110 of those places carry a comment that starts <code>PARITY</code>. Its "
      "yardstick is not 5.2.4 but its own carried copy of the 5.3.0-beta.3 legacy engine. The regression suite "
      "holds the new engine to that copy within 0.001 absolute or 0.1 percent on EPA's Example1, Example2 and "
      "Example3, and within 0.05 absolute or 1 percent on one slot-surcharge deck."),
('p', "The first thing anyone notices is the size. One short script, <code>tools/loc_buckets.py</code>, counts "
      "every C and C++ line in the three engines by the same rule, on SWMM6's <code>swmm6_rel</code> branch at "
      "commit <code>c40580c</code>. EPA SWMM 5.2.4's engine is 50,454 lines. swmm5-cplus is 53,167, five "
      "percent more. SWMM6's engine, leaving out its 2D code, is 233,434: four point six times EPA's."),
('fig', 'swmm6_size', "Lines of C and C++ in EPA 5.2.4, swmm5-cplus and SWMM6's engine (src/engine without 2D, "
                      "swmm6_rel at c40580c), with SWMM6 split by what its files do. The jobs that all of 5.2.4 "
                      "does take SWMM6 about twice as many lines; the other 58 percent is work 5.2.4 does not do."),
('p', "Comments explain some of it. SWMM6 is written in the documented style a modern library is expected to "
      "have: a license block at the top of every file and a Doxygen paragraph over every function, with 15,205 "
      "lines that are Doxygen tags alone. Comments are 35 percent of its engine lines against 27 percent of "
      "EPA's. Strip out every comment and blank line and the ratio hardly moves: 134,798 lines of code against "
      "30,561, four point four times as much. The size is in the code."),
('p', "So what is the code for? Sorting SWMM6's 468 engine files into five piles by file name and folder gives "
      "this:"),
('table', ['Share', 'What the files do', 'Lines', 'Code lines only'], [
    ['42%', 'The same jobs 5.2.4 does, rewritten', '98,745', '51,054'],
    ['26%', 'New physics that 5.2.4 does not have', '61,083', '35,258'],
    ['16%', 'Save/restore, live editing, file interchange', '38,090', '25,333'],
    ['11%', 'Public API implementation', '24,557', '17,902'],
    ['5%', 'Plugins, thread bookkeeping, timers, errors, paths', '10,959', '5,251'],
]),
('p', "SWMM6 has 104,237 more lines of code than 5.2.4. About four fifths of that extra code does things 5.2.4 "
      "does not do at all. The other fifth is 5.2.4's own work, done in a bulkier way. Take the piles one at a "
      "time."),
('ul', [
    "<b>New physics, 61,083 lines.</b> This one pile is bigger than all of EPA 5.2.4. It holds finite-volume "
    "routing, with an implicit network solver that brings its own GMRES and ILU; heat transport; an Eulerian "
    "transport engine and MSX-style reactions; water age; Lagrangian quality; Richards-equation LID columns; "
    "and the Utah Energy Balance snow model, translated from Fortran, whose one reference file is 3,247 lines.",
    "<b>Save/restore, live editing and file interchange, 38,090 lines.</b> Every subsystem can write its state "
    "to a checkpoint and read it back, and the checkpoint and hot-start files come to 19,851 lines across the "
    "engine, 2D included. 5.2.4's hot-start file does part of that job in the 544 lines of "
    "<code>hotstart.c</code>. The rest 5.2.4 does not do at all: SWMM6 writes a model back out as an "
    "<code>.inp</code> (<code>InpWriter.cpp</code>, 5,043 lines), deletes objects from a loaded model, and reads "
    "and writes GeoPackage, whose reader, writer and schema alone are 8,172 lines.",
    "<b>The public API, 24,557 lines.</b> SWMM6's headers declare 1,419 distinct <code>swmm_</code> functions. "
    "Each one checks its handle, its index and the state of the run before it touches anything, and returns an "
    "error code. EPA 5.2.4 exports 20, from a <code>swmm5.c</code> of 1,721 lines.",
    "<b>Plugins, thread bookkeeping, timers, errors and paths, 10,959 lines.</b> The machinery any library needs "
    "and a command-line program can do without.",
    "<b>The same jobs, rewritten, 98,745 lines.</b> Here SWMM6 spends 51,054 lines of code where 5.2.4 spends "
    "30,561 on everything, 1.7 times as much. Three examples show where it goes. The file that drives a run, "
    "<code>SWMMEngine.cpp</code>, is 11,731 lines by itself. Keeping each quantity in its own array, the "
    "structure-of-arrays layout built for vector units and threads, means state headers of 1,265 to 1,449 lines "
    "each for links, nodes and subcatchments. And cross-section geometry, 2,618 lines in EPA's "
    "<code>xsect.c</code>, now spreads over six files and 7,280 lines, because the same formulas must run in "
    "three places. Its header says so: one implementation, three consumers, host dynamic wave, host finite "
    "volume and device finite volume. A GPU kernel cannot reach the host's global tables, so the formulas had "
    "to move where it can.",
]),
('p', "Outside the engine count there is as much again."),
('fig', 'swmm6_repo', "The rest of the SWMM6 repository by folder. The unit tests alone are 199,608 lines, nearly "
                      "the size of the engine; EPA 5.2.4's test folder is 364. The 33,790-line file is C that "
                      "Cython generated for the legacy engine's Python binding, not code anyone wrote by hand."),
('p', "The tests are the telling number. Two hundred thousand lines of tests for 233 thousand lines of engine "
      "describe a project that expects to change often and wants to know at once when a change breaks "
      "something. The rest is the 79,357 lines of 2D, the 58,853-line legacy engine carried as a yardstick, and "
      "33,176 lines of public headers, more than three quarters of them documentation."),
('p', "Where should a reviewer look? Not at the new physics, which has no older answer to match, but at the "
      "98,745 lines that redo 5.2.4's jobs. That is where 109 of the 110 <code>PARITY</code> comments sit, 27 of "
      "them in <code>DynamicWave.cpp</code>. Each marks a place where the new code had to be bent back to give a "
      "legacy result exactly. Here is one:"),
('code', 'cpp', """// PARITY: legacy link_getFroude computes sqrt(GRAVITY * y) directly
// (link.c). Using the precomputed SQRT_GRAVITY constant (a truncated
// sqrt(32.2)) times sqrt(dh) differs by ~3e-9 and reorders the FP
// ops; that shifts Froude across the sigma (0.5/1.0) and SUB/SUPER
// critical knife-edges, amplifying into macroscopic transients.
fr = (dh > 0.0) ? absv / std::sqrt(constants::GRAVITY * dh) : 0.0;"""),
('p', "A square root taken a different way moved the Froude number by three parts in a billion. That was enough "
      "to push links across the Froude thresholds where inertial damping switches on and off, and where flow is "
      "classed sub- or supercritical, and the difference grew into transients anyone could see. swmm5-cplus has "
      "no comments like that, because it never changed the arithmetic; its proof is the byte comparison in the "
      "Verify tab. The price runs the other way. swmm5-cplus still has EPA's memory layout, and SWMM6 has one "
      "built for speed and threads. Which of the two is faster, and how close SWMM6 comes to 5.2.4 across the "
      "corpus, takes building and running SWMM6. The sections that follow do that."),
('p', "SWMM6 is OpenSWMM 6.0.0-alpha from HydroCouple, under the Apache License 2.0, at "
      "github.com/HydroCouple/openswmm.engine. Caleb Buahin is lead developer, with Corinne Wiesner-Friedman on "
      "development and documentation. The excerpt above is from its <code>DynamicWave.cpp</code> at commit "
      "<code>c40580c</code>."),

('h2', 'Records against columns', 's6layout'),
('p', "Size is the least of the differences. Set side by side, the three engines also keep their state, report their errors and check their answers in different ways:"),
('table', ['', 'EPA 5.2.4', 'swmm5-cplus', 'SWMM6'], [
    ['Engine source, lines', '{EPA_LINES}', '{CP_LINES}', '{S6_LINES} plus {S6_LINES_2D} for 2D'],
    ['Where model state lives', 'global arrays: <code>Node[j].newDepth</code>', 'the same arrays, inside one <code>Project</code>', 'one array per quantity: <code>ctx.nodes.depth[j]</code>'],
    ['Public C API', '{EPA_API} functions', 'the same {EPA_API}, plus {CP_HANDLE} <code>swmm5cp_</code> handle functions', '{S6_API} <code>swmm_</code> functions, plus Python'],
    ['Errors', 'error codes', 'error codes', 'error codes at the API; {S6_THROW} <code>throw</code> statements inside'],
    ['Threads', 'OpenMP loops, off by default', 'none', '{S6_OMP} OpenMP pragmas, a background output thread'],
    ['New physics', 'none', 'none', 'finite-volume routing, virtual junctions, dynamic slot, 2D, new quality solvers, heat'],
    ['Checked against', 'itself: it is the reference', 'EPA 5.2.4, byte for byte, {CORPUS_N_SHORT} corpus models', 'its own legacy engine, {S6_REG_N} decks, within {S6_REG_TOL}'],
]),
('p', "EPA SWMM keeps a node as a record: depth, head, volume, inflow and some thirty other fields, side by side in "
      "one C struct of {EPA_NODE_BYTES} bytes, one struct per node. A link record is {EPA_LINK_BYTES} bytes. swmm5-cplus kept that layout on purpose, because moving a field changes "
      "nothing the compiler computes but everything a reviewer has to check. SWMM6 turned it on its side. It keeps one "
      "array for every quantity, all the depths together and all the heads together, which is what database people call a "
      "column store and game programmers call a structure of arrays."),
('fig', 's6_layout', "The same node data, two ways. A loop that reads every node's depth touches one double in "
                     "each record of the first layout, and consecutive doubles in the second."),
('code', 'cpp', """// EPA 5.2.4 and swmm5-cplus: one record per node
Node[j].newDepth

// SWMM6: one array per quantity, owned by the simulation context
ctx.nodes.depth[j]"""),
('p', "The point is speed. A dynamic-wave iteration reads a handful of fields from every node and link, again and "
      "again, and a processor fetches memory in 64-byte lines. When the depths sit together, one fetch brings in eight of "
      "them; when each depth sits inside a {EPA_NODE_BYTES}-byte record, one fetch brings in one depth and a lot of "
      "fields nobody asked for. Columns also let the compiler run one instruction over several elements at once. SWMM6 "
      "goes further: it copies each conduit's fixed data into a tile packed in conduit order, groups conduits by "
      "shape so that each shape's geometry runs as one tight loop, and can split the iteration across threads with "
      "OpenMP, though by default it uses one."),
('h2', 'What SWMM6 checks itself against', 's6parity'),
('p', "Before running anything, read the tests. SWMM6 carries a second engine, which it calls legacy: EPA's C code "
      "at version 5.3.0-beta.3, built from the same tree. That engine is not 5.2.4. Compared token by token with "
      "comments and layout removed, its <code>dwflow.c</code> differs from 5.2.4 in {S6_LEG_DWFLOW} of {S6_LEG_DWFLOW_N} tokens and "
      "its <code>routing.c</code> in {S6_LEG_ROUTING} of {S6_LEG_ROUTING_N}; its <code>xsect.c</code> is untouched. Much of that is "
      "diagnostic tracing, file writes switched on by environment variables. Three changes alter results. In the "
      "momentum equation a conveyance area leaves out the Preissmann slot, which matters only under SURCHARGE_METHOD "
      "SLOT with a pipe above its crown. A storage node whose links contribute no head derivative is no longer treated "
      "as surcharged. And a free or normal outfall at the end of a pipe that sits above the outfall's invert gets a "
      "different depth. Here is 5.2.4, <code>node.c</code>:"),
('code', 'c', """case FREE_OUTFALL:
    if ( z > 0.0 ) Node[j].newDepth = 0.0;
    else Node[j].newDepth = MIN(yNorm, yCrit);
    return;"""),
('p', "Here <code>z</code> is the pipe's invert offset above the outfall. And here is the legacy engine SWMM6 tests against:"),
('code', 'c', """case FREE_OUTFALL:
    Node[j].newDepth = z + MIN(yNorm, yCrit);
    return;"""),
('p', "Both are defensible. 5.2.4 says that water falling freely out of a pipe leaves nothing standing at the "
      "outfall; 5.3 reports the water surface where it leaves the pipe. They are different numbers for the outfall depth and head, and SWMM6's "
      "new engine follows the legacy rule. {S6_OF_TXT}"),
('p', "The new engine is held to that legacy engine, not to EPA, by the regression test described above. A second "
      "check, the repository's bit-identity corpus, runs {S6_CORPUS_DECKS} decks through two builds of SWMM6 and demands "
      "identical files, which answers whether a change moved anything it should not have. It compares SWMM6 with itself, "
      "and {S6_CORPUS_FV} of its decks use the new finite-volume router. Its own README says plainly what that does and "
      "does not prove, which is to its credit."),
('fig', 's6_parity', "Who is compared with whom. swmm5-cplus is held to EPA 5.2.4 byte for byte. SWMM6's new engine is "
                     "held to its legacy engine, a newer EPA version than 5.2.4. The counts on the solid arrows come from the "
                     "four-engine gate below."),
('h2', 'Four engines, one corpus', 's6gate'),
('p', "So I built SWMM6 the way its repository builds it, Release with link-time optimization and "
      "<code>-ffp-contract=off</code>, with the optional GeoPackage, HDF5, 2D and GPU modules turned off, and ran four "
      "engines on the same models: EPA 5.2.4, swmm5-cplus, SWMM6's legacy engine and SWMM6's new engine. The model set "
      "is every corpus model that took EPA under five seconds in the swmm5-cplus gate, {S6_MODELS} of them, plus a seeded "
      "sample of {S6_B_MODELS} larger ones. Each SWMM6 engine ran every model twice, because an engine that does not reproduce itself cannot "
      "be compared with anything. Then every reported value in every reporting period was compared, pair by pair."),
('p', "The controls came out clean. swmm5-cplus matched EPA on {S6_EC} of {S6_EC_N} models that wrote results; SWMM6's "
      "legacy engine matched its own rerun on {S6_LL} of {S6_LL_N}, and the new engine matched its own rerun on {S6_NN} of {S6_NN_N}. "
      "One difference in format had to be handled before anything else: on {S6_EXTRA} models EPA 5.2.4 drops the "
      "final reporting period, at the end time, and both SWMM6 engines write it. Periods are matched by their time "
      "stamps, and only periods present in both files are compared."),
('fig', 's6_classes', "Each model in one band, by the largest change in any link flow or node depth over the run as a "
                      "share of that element's peak, above 1 mm and 1 L/s. Controls first, then the three comparisons that matter."),
('p', "Then the comparisons that matter. SWMM6's legacy engine reproduced EPA 5.2.4 on {S6_EL} of {S6_EL_N} models that "
      "both ran. Its new engine reproduced the legacy engine on {S6_LN} of {S6_LN_N}, and EPA 5.2.4 on {S6_EN} of {S6_EN_N}. "
      "Where the new engine and its legacy engine differ, {S6_LN_LAST} models differ only in the last digits, "
      "{S6_LN_SMALL} by less than one percent of some element's peak, {S6_LN_MID} by one to ten percent and {S6_LN_BIG} by more; "
      "in {S6_LN_OFF} only system totals, subcatchments or elements below the floor differ."),
('p', "That band is a strict test: one element out of hundreds is enough to put a model over ten percent. Seen from the "
      "outlet the picture is calmer. Where the new engine differs from EPA 5.2.4, the total outflow volume agrees within "
      "0.1 percent in {S6_EN_VOL01} of {S6_EN_VOLN} models and within 1 percent in {S6_EN_VOL1}; the peak outflow agrees within "
      "1 percent in {S6_EN_PK1}. Where the new engine differs from its own legacy engine, the outflow volume agrees within "
      "0.1 percent in {S6_LN_VOL01} of {S6_LN_VOLN}."),
('p', "A difference in the last digits is how dynamic wave diverges, so it is worth watching one grow. Take a "
      "27-junction training model, {S6_TR_MODEL}. SWMM6 ships trace switches in both of its engines, environment variables "
      "that write a fingerprint of the whole hydraulic state after every routing step, and they put the first difference "
      "at step {S6_TR_STEP} of about {S6_TR_STEPS}, {S6_TR_T} seconds into the run. The hash of every flow and depth "
      "differs; their sums do not. Dumping that step element by element, the lowest-numbered link that differs is "
      "conduit {S6_TR_LINK}, and in its first traced iteration the upstream flow area, the flow and the flow-head "
      "derivative each differ by one or two units in the last place. The binary output stores 32-bit numbers, so "
      "nothing shows there until {S6_TR_FIRST} minutes in, on the falling limb of the first storm, when junction depths "
      "differ in the third decimal. At worst the two engines are {S6_TR_DQ} cfs apart on a peak of {S6_TR_Q} cfs and "
      "{S6_TR_DY} ft apart in depth. Between storms the gap closes completely: in {S6_TR_SAME} of the {S6_TR_PER} "
      "reporting periods every node depth and link flow is identical again, because the network drains and forgets."),
('fig', 's6_growth', "The largest link-flow difference between SWMM6's two engines in each five-minute reporting period of "
                     "{S6_TR_MODEL_TXT}, on a log scale, over the largest link flow. It appears at the storm peaks and vanishes between them."),
('p', "I could not pin that first difference to a line of code. Both engines compute the area with the same formula, "
      "and SWMM6's batch geometry kernel is written operation for operation like EPA's <code>lookup()</code>. But "
      "neither traced area equals that formula applied to the traced depth, so the trace does not show the depth the "
      "area was computed from. That is the honest state of it: located to a step, a link and a variable, not to a line."),
('p', "Some models one engine runs and another refuses. {S6_N_REFUSE_TXT} {S6_SIX_TXT}"),
('h2', 'Speed, and what each could borrow', 's6speed'),
('p', "Speed is where the columns were supposed to pay, so I timed the four engines the same way the speed section "
      "timed swmm5-cplus: the same eight models, CPU time, three interleaved runs, and EPA timed against itself for the "
      "noise, which came out at {S6_SP_NOISE} per model. Each engine was built the way its own project builds it. The numbers do not "
      "care what anybody expected. SWMM6's new engine took {S6_SP_SWMM6_NEW} times EPA's CPU time on average, from "
      "{S6_SP_SWMM6_NEW_RANGE} across the models; CPU time counts its background output thread too, and by the clock "
      "the average was {S6_SP_WALL}. Its legacy engine took {S6_SP_SWMM6_LEGACY}. swmm5-cplus R4 took "
      "{S6_SP_CPLUS_LTO} built plainly and {S6_SP_CPLUS_PGO} with the profile-guided build, every byte the same as EPA's."),
('fig', 's6_speed', "CPU time against EPA 5.2.4 on the eight timing models of the speed section, three interleaved runs, "
                    "each engine built the way its own project builds it."),
('p', "Those eight models are small, the largest a few hundred links, and columns are meant to pay on big networks. So "
      "I timed {S6_LG_N} larger ones the same way, from {S6_LG_MIN} to {S6_LG_MAX} links, with the noise this time at "
      "{S6_LG_NOISE}. The gap narrowed. On the four networks of {S6_LG_MIDLINKS} links SWMM6's new engine took {S6_LG_MID} "
      "times EPA's CPU time, and on the {S6_LG_MAX}-link network it took {S6_LG_BIG}: faster than EPA, by more than the "
      "noise, while taking slightly more routing steps. The legacy engine averaged {S6_LG_LEG} on these five and "
      "swmm5-cplus {S6_LG_CP}."),
('p', "To see where the time goes, I counted instructions with callgrind, which does not care what else the machine "
      "is doing, on the model with the widest gap: {S6_PR_MODEL}, eight months of continuous simulation in five-minute steps. "
      "EPA executed {S6_PR_EPA} billion instructions; SWMM6's new engine executed {S6_PR_NEW} billion. The hydraulics are "
      "not where the difference lies. Routing and cross-section geometry took {S6_PR_HYD_EPA} billion in EPA and "
      "{S6_PR_HYD_NEW} billion in SWMM6. Writing results took {S6_PR_OUT_EPA} billion in EPA and {S6_PR_OUT_NEW} billion in "
      "SWMM6, and allocating and copying memory {S6_PR_MEM_EPA} against {S6_PR_MEM_NEW}. At every reporting period SWMM6 copies "
      "its state into a snapshot and hands it to a background thread, and that thread writes every number with a call "
      "of its own, then flushes the file so that a live reader can see the new period. Here is SWMM6's "
      "<code>DefaultOutputPlugin.cpp</code>:"),
('code', 'cpp', """void DefaultOutputPlugin::writeReal4(float value) {
    std::fwrite(&value, sizeof(float), 1, out_file_);
}"""),
('p', "EPA's <code>output.c</code> writes each element's values with one call:"),
('code', 'c', """fwrite(NodeResults, sizeof(REAL4), NumNodeVars, file);"""),
('p', "{S6_BUF_TXT}"),
('p', "There is one thing SWMM6 could take from swmm5-cplus today. SWMM6 sets its outfall depths inside the iteration "
      "loop, as EPA does, and it solves normal and critical depth every time, as EPA does. From its "
      "<code>Outfall.cpp</code>:"),
('code', 'cpp', """double q = std::fabs(ctx.links.flow[uk]) / barrels;
XSectParams xs = buildXSectParams(ctx, uk);
yNorm = getYnorm(xs, beta_v, qmax_v, q);
yCrit = xsect::getYcrit(xs, q);"""),
('p', "That is the call the R4 memo removes when the flow repeats, without changing a bit. It carries straight over, "
      "with one addition: SWMM6's dynamic slot can change a pipe's geometry during a run, so the remembered answer has "
      "to remember the geometry too. What it would save in SWMM6 was not measured."),
('p', "So the columns are aimed at the right place, and on a big network they already pay. The speed section found the "
      "time going into cross sections and the math library inside the iteration, and that is what SWMM6's columns, "
      "conduit tiles and shape-grouped geometry are built to make cheaper. On a network of eight thousand links that "
      "shows. On a network of a few hundred, the cost of everything around the solver, which the profile above found in "
      "snapshots, allocation and statistics, still outweighs it, and on the smallest models it swamps it. The honest summary is a split "
      "decision. swmm5-cplus kept EPA's arithmetic and EPA's layout and runs at EPA's speed or a little better, with "
      "byte-identical results, at every size tried. SWMM6 changed the layout, added a great deal, wins on the biggest network here and "
      "loses on the small ones, and is still earning back the arithmetic. The engine that does both, columns with EPA's "
      "arithmetic kept exactly and a byte gate behind every change, has not been built by anybody yet."),

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
    "The SWMM6 line counts are a reading of its source at one commit. Lines were counted by one simple rule for all "
    "three engines, a code line with a trailing comment counts as code, and SWMM6's files were sorted into piles by "
    "file name, so each share is good to a few percent.",
    "The SWMM6 comparison is of one commit, {S6_COMMIT}, read and run on {S6_DATE}; SWMM6 is in alpha and changes daily. "
    "It was built without its optional GeoPackage, HDF5, 2D and GPU modules, so none of those were run, and the corpus "
    "is SWMM5 models, which exercise SWMM6's legacy-compatible paths and almost none of its new physics. SWMM6 measures "
    "itself against its own legacy engine, not against 5.2.4, so a difference from 5.2.4 is a difference, not a defect. Its speed was "
    "timed on the eight models of the speed section and five larger ones on the same two-core machine, and the "
    "instruction profile is of one model; nothing larger than 8,683 links was timed.",
    "Agreement is measured on the binary output, which stores 32-bit numbers at each reporting step. Two engines that "
    "agree there may still differ in the last bits of their 64-bit state, as the trace in that section shows. Agreement "
    "says nothing about which engine is closer to the real system; no field data were used.",
    "The attribution of the legacy engine's differences rests on patching EPA 5.2.4 with three of its changes and "
    "rerunning; the rest of its changes, in {S6_LEG_FILES} of {S6_LEG_FILES_N} source files, were counted but not read line by line.",
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
      "experiments, wrote the outfall memo that became R4 and the speed section above. That evening two Claude sessions "
      "worked on SWMM6 side by side. One read its source, wrote <code>tools/loc_buckets.py</code> to count the three "
      "engines, and wrote the section on the other C++ SWMM. The other, with Bob's go-ahead to build and run SWMM6's "
      "code, built its two engines, wrote the four-engine gate, the attribution patches, the trace and profile "
      "analysis and the output-writer test, and wrote the sections from Records against columns onward."),
('p', "<b>How it was checked.</b> The checks are mechanical, not the AI's judgment: byte comparison of "
      "<code>.out</code> and <code>.rpt</code> files against EPA SWMM 5.2.4 built from the official tag, "
      "AddressSanitizer runs, and interleaved timing with a noise floor. <b>What no person has checked yet:</b> "
      "Bob has not yet reviewed the generated C++ or the hand-written changes line by line, nor rerun the gate "
      "on his own machine."),
]
