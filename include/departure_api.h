#pragma once

#include <Arduino.h>
#include <vector>

#include "config.h"
#include "departure.h"

class DepartureApi {
   public:
    DepartureApi(const char* baseUrl, const char* stopId);

    // Fuer VRR EFA wird zusaetzlich der Ortsname benoetigt.
    DepartureApi(const char* baseUrl, const char* stopPlace, const char* stopId);

    bool fetch(std::vector<Departure>& out);

   private:
    String buildUrl();

    String baseUrl_;
    String stopPlace_;
    String stopId_;
};
