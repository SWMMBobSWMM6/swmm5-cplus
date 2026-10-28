// coupled.cpp -- two SWMM models running in one process, coupled at a node.
//
// The upstream model's outfall OA feeds the downstream model's node IN: after each
// lock-step routing step, the flow arriving at OA is handed to IN as lateral inflow.
// The same network built as one model (merged.inp) is the check.
//
// EPA SWMM 5.2.4 keeps its state in process-wide globals, so a program can hold only
// one open model; this coupling needed two processes and IPC. With swmm5-cplus it is
// two objects.
//
//   coupled <models dir> <out dir>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "swmm5.h"
#include "swmm5cp.hpp"

int main(int argc, char** argv)
{
    std::string dir = argc > 1 ? argv[1] : "examples/models";
    std::string out = argc > 2 ? argv[2] : "/tmp";
    swmm::Simulation up, dn;

    if (up.open(dir + "/upstream.inp", out + "/up.rpt", out + "/up.out") || up.start())
        { std::printf("upstream: %s\n", up.error().c_str()); return 1; }
    if (dn.open(dir + "/downstream.inp", out + "/dn.rpt", out + "/dn.out") || dn.start())
        { std::printf("downstream: %s\n", dn.error().c_str()); return 1; }
    int oa = up.index(swmm_NODE, "OA");
    int in = dn.index(swmm_NODE, "IN");

    // lock-step: both models use the same fixed 5-second routing step
    double tu = 0, td = 0;
    long steps = 0;
    for (;;)
    {
        bool a = up.step(tu);
        dn.setValue(swmm_NODE_LATFLOW, in, up.value(swmm_NODE_INFLOW, oa));
        bool b = dn.step(td);
        steps++;
        if (std::fabs(tu - td) > 1e-12) { std::printf("clocks diverged at step %ld\n", steps); return 2; }
        if (!a || !b) break;
    }
    up.end();
    dn.end();

    // compare outfall OB inflow, coupled vs merged, at every reporting period
    swmm::Simulation chk;      // a third model, same process
    chk.open(dir + "/merged.inp", out + "/merged.rpt", out + "/merged.out");
    chk.start(); double t; while (chk.step(t)) {} chk.end();
    int obD = dn.index(swmm_NODE, "OB"), obM = chk.index(swmm_NODE, "OB");
    double peakD = 0, peakM = 0, sumD = 0, sumM = 0, maxAbs = 0;
    for (int k = 1; ; k++)
    {
        double qd = dn.savedValue(swmm_NODE_INFLOW, obD, k);
        double qm = chk.savedValue(swmm_NODE_INFLOW, obM, k);
        if (k > 144) break;
        peakD = std::fmax(peakD, qd); peakM = std::fmax(peakM, qm);
        sumD += qd; sumM += qm;
        maxAbs = std::fmax(maxAbs, std::fabs(qd - qm));
    }
    std::printf("steps %ld (both models)\n", steps);
    std::printf("OB peak inflow   coupled %.2f cfs   merged %.2f cfs   diff %.2f%%\n",
                peakD, peakM, 100.0 * (peakD - peakM) / peakM);
    std::printf("OB volume (sum)  coupled %.1f   merged %.1f   diff %.2f%%\n",
                sumD, sumM, 100.0 * (sumD - sumM) / sumM);
    std::printf("largest 5-min difference %.2f cfs\n", maxAbs);
    return 0;
}
