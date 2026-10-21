#pragma once
struct State
{
    bool isShuttingDown = false;
    void* currentCommandQueue = nullptr; // timestamp queries in the standalone fixture
    static State& Instance()
    {
        static State state;
        return state;
    }
};
