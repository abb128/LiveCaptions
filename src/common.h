#pragma once

#define MINIMUM_BENCHMARK_RESULT (1.2)


#define GET_MODEL_PATH() (getenv("APRIL_MODEL_PATH") == NULL) ? "/app/LiveCaptions/models/aprilv0_en-us.april" : getenv("APRIL_MODEL_PATH")