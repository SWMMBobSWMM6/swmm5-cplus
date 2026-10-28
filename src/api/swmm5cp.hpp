//-----------------------------------------------------------------------------
//   swmm5cp.hpp  --  swmm5-cplus C++ interface (header-only, over swmm5cp.h)
//
//   swmm::Simulation owns one Project. It is movable, not copyable, and closes
//   and frees its project when it goes out of scope, whatever state it is in.
//
//       swmm::Simulation sim;
//       if (sim.open("site.inp", "site.rpt", "site.out") || sim.start())
//           return report(sim.error());
//       double days;
//       while (sim.step(days))
//           q.push_back(sim.value(swmm_LINK_FLOW, outletLink));
//       sim.end();
//
//   Any number of Simulations may be alive at once, in one thread or many (one
//   thread per Simulation at a time). EPA SWMM 5.2.4 allowed exactly one per
//   process.
//-----------------------------------------------------------------------------
#ifndef SWMM5CP_HPP
#define SWMM5CP_HPP

#include <string>
#include <utility>
#include "swmm5cp.h"

namespace swmm {

class Simulation
{
public:
    Simulation() : h_(swmm5cp_create()) {}
    ~Simulation() { if (h_) swmm5cp_destroy(h_); }

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&& o) noexcept : h_(o.h_), rc_(o.rc_) { o.h_ = nullptr; }
    Simulation& operator=(Simulation&& o) noexcept
    {
        if (this != &o) { if (h_) swmm5cp_destroy(h_); h_ = o.h_; rc_ = o.rc_; o.h_ = nullptr; }
        return *this;
    }

    // whole run in one call; returns the EPA error code (0 = success)
    int run(const std::string& inp, const std::string& rpt, const std::string& out = "")
        { return rc_ = swmm5cp_run(h_, inp.c_str(), rpt.c_str(), out.c_str()); }

    // stepwise run
    int open(const std::string& inp, const std::string& rpt, const std::string& out = "")
        { return rc_ = swmm5cp_open(h_, inp.c_str(), rpt.c_str(), out.c_str()); }
    int start(bool saveResults = true) { return rc_ = swmm5cp_start(h_, saveResults ? 1 : 0); }

    // advances one routing step; false once the simulation is over or on error
    bool step(double& elapsedDays)
    {
        rc_ = swmm5cp_step(h_, &elapsedDays);
        return rc_ == 0 && elapsedDays > 0.0;
    }
    int end()    { return rc_ = swmm5cp_end(h_); }
    int report() { return rc_ = swmm5cp_report(h_); }
    int close()  { return swmm5cp_close(h_); }

    // results and properties (swmm5.h property codes)
    double value(int property, int index) const { return swmm5cp_getValue(h_, property, index); }
    void   setValue(int property, int index, double v) { swmm5cp_setValue(h_, property, index, v); }
    double savedValue(int property, int index, int period) const
        { return swmm5cp_getSavedValue(h_, property, index, period); }
    int    count(int objType) const { return swmm5cp_getCount(h_, objType); }
    int    index(int objType, const std::string& id) const
        { return swmm5cp_getIndex(h_, objType, id.c_str()); }
    std::string name(int objType, int idx) const
    {
        char buf[64] = {0};
        swmm5cp_getName(h_, objType, idx, buf, (int)sizeof(buf) - 1);
        return buf;
    }

    // status
    int  errorCode() const { return rc_; }
    std::string error() const
    {
        char buf[256] = {0};
        swmm5cp_getError(h_, buf, (int)sizeof(buf) - 1);
        return buf;
    }
    bool massBalance(float& runoff, float& flow, float& qual) const
        { return swmm5cp_getMassBalErr(h_, &runoff, &flow, &qual) == 0; }
    int  warnings() const { return swmm5cp_getWarnings(h_); }
    static int version() { return swmm5cp_getVersion(); }

    swmm5cp_handle handle() const { return h_; }

private:
    swmm5cp_handle h_ = nullptr;
    int rc_ = 0;
};

} // namespace swmm
#endif
