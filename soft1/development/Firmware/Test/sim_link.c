#include "mock.h"
#include "profile.h"
#include "service.h"
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

typedef struct {
    Mock model;
    Safety safety;
    Params params;
    Parser parser;
    int failed;
} SimLink;

static void dispatch(void *context, const Frame *request)
{
    SimLink *sim = context;
    const Devices d = mock_devices(&sim->model);
    sim->model.now_ms += 50u;
    Snapshot snap = {.safety = &sim->safety, .now_ms = sim->model.now_ms,
                     .good_frames = sim->parser.good, .bad_frames = sim->parser.bad};
    d.voltage(d.context, 0u, &snap.bus_mv);
    Frame reply;
    uint8_t bytes[RM_FRAME_MAX];
    debug_handle(&sim->params, &sim->safety, &snap, OWNER_PC, false, request, &reply);
    const size_t n = protocol_encode(&reply, bytes, sizeof(bytes));
    if (n == 0u || fwrite(bytes, 1u, n, stdout) != n || fflush(stdout) != 0) sim->failed = 1;
}

int main(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 ||
        _setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    SimLink sim = {0};
    params_reset(&sim.params);
    safety_begin(&sim.safety);
    safety_self_test(&sim.safety, true);
    for (int byte = getchar(); byte != EOF && !sim.failed; byte = getchar()) {
        protocol_feed(&sim.parser, (uint8_t)byte, sim.model.now_ms, dispatch, &sim);
    }
    return sim.failed || ferror(stdin);
}
