// multi_native.cpp -- swmm5-cplus isolation test (native)
//
//   multi_native rr  <outdir> a.inp b.inp ...   open all in one process, step round-robin
//   multi_native par <outdir> a.inp b.inp ...   one std::thread per model, all at once
//
// Each model writes <outdir>/<name>.rpt/.out. Compare against EPA runs afterwards.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include "swmm5cp.h"

static std::string base(const std::string& p)
{
    size_t s = p.find_last_of('/');
    std::string b = (s == std::string::npos) ? p : p.substr(s + 1);
    size_t d = b.rfind('.');
    return d == std::string::npos ? b : b.substr(0, d);
}

int main(int argc, char** argv)
{
    if (argc < 4) { std::fprintf(stderr, "usage: multi_native rr|par outdir a.inp ...\n"); return 1; }
    std::string mode = argv[1], outdir = argv[2];
    std::vector<std::string> inps(argv + 3, argv + argc);
    size_t n = inps.size();

    if (mode == "par")
    {
        std::vector<std::thread> T;
        std::vector<int> rc(n, 0);
        for (size_t i = 0; i < n; i++)
            T.emplace_back([&, i] {
                swmm5cp_handle h = swmm5cp_create();
                std::string b = outdir + "/" + base(inps[i]);
                rc[i] = swmm5cp_run(h, inps[i].c_str(), (b + ".rpt").c_str(), (b + ".out").c_str());
                swmm5cp_destroy(h);
            });
        for (auto& t : T) t.join();
        for (size_t i = 0; i < n; i++) std::printf("%s rc=%d\n", base(inps[i]).c_str(), rc[i]);
        return 0;
    }

    std::vector<swmm5cp_handle> H(n);
    std::vector<int> rc(n, 0), done(n, 0);
    std::vector<long> steps(n, 0);
    for (size_t i = 0; i < n; i++)
    {
        H[i] = swmm5cp_create();
        std::string b = outdir + "/" + base(inps[i]);
        rc[i] = swmm5cp_open(H[i], inps[i].c_str(), (b + ".rpt").c_str(), (b + ".out").c_str());
        if (!rc[i]) rc[i] = swmm5cp_start(H[i], 1);
        done[i] = rc[i] != 0;
    }
    std::printf("live projects: %d\n", swmm5cp_count());
    for (bool any = true; any; )
    {
        any = false;
        for (size_t i = 0; i < n; i++)
        {
            if (done[i]) continue;
            double t = 0.0;
            int r = swmm5cp_step(H[i], &t);
            steps[i]++;
            if (r || t <= 0.0) { done[i] = 1; if (r) rc[i] = r; }
            else any = true;
        }
    }
    for (size_t i = 0; i < n; i++)
    {
        swmm5cp_end(H[i]);
        // like swmm_run: with a saved .out file the time-series report is not written
        swmm5cp_close(H[i]);
        swmm5cp_destroy(H[i]);
        std::printf("%s rc=%d steps=%ld\n", base(inps[i]).c_str(), rc[i], steps[i]);
    }
    std::printf("live projects after: %d\n", swmm5cp_count());
    return 0;
}
