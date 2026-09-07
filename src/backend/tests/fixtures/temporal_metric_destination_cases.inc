// Public runtime expectations for the six metric destination options.
{"D|single:arrival_rate->@@arrival_rate", {{&LogToFeaturesOptions::arrival_rate, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:arrival_rate->@@finish_rate", {{&LogToFeaturesOptions::arrival_rate, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 28.333333333333332, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:arrival_rate->@@service_time", {{&LogToFeaturesOptions::arrival_rate, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:arrival_rate->@@waiting_time", {{&LogToFeaturesOptions::arrival_rate, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:arrival_rate->@@sojourn_time", {{&LogToFeaturesOptions::arrival_rate, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:arrival_rate->@@diff_start_end", {{&LogToFeaturesOptions::arrival_rate, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@arrival_rate", {{&LogToFeaturesOptions::finish_rate, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 28.333333333333332, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@finish_rate", {{&LogToFeaturesOptions::finish_rate, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@service_time", {{&LogToFeaturesOptions::finish_rate, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@waiting_time", {{&LogToFeaturesOptions::finish_rate, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@sojourn_time", {{&LogToFeaturesOptions::finish_rate, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:finish_rate->@@diff_start_end", {{&LogToFeaturesOptions::finish_rate, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@arrival_rate", {{&LogToFeaturesOptions::service_time, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@finish_rate", {{&LogToFeaturesOptions::service_time, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@service_time", {{&LogToFeaturesOptions::service_time, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@waiting_time", {{&LogToFeaturesOptions::service_time, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@sojourn_time", {{&LogToFeaturesOptions::service_time, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 0, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:service_time->@@diff_start_end", {{&LogToFeaturesOptions::service_time, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@arrival_rate", {{&LogToFeaturesOptions::waiting_time, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@finish_rate", {{&LogToFeaturesOptions::waiting_time, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@service_time", {{&LogToFeaturesOptions::waiting_time, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@waiting_time", {{&LogToFeaturesOptions::waiting_time, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@sojourn_time", {{&LogToFeaturesOptions::waiting_time, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:waiting_time->@@diff_start_end", {{&LogToFeaturesOptions::waiting_time, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@arrival_rate", {{&LogToFeaturesOptions::sojourn_time, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@finish_rate", {{&LogToFeaturesOptions::sojourn_time, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@service_time", {{&LogToFeaturesOptions::sojourn_time, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 0, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@waiting_time", {{&LogToFeaturesOptions::sojourn_time, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@sojourn_time", {{&LogToFeaturesOptions::sojourn_time, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:sojourn_time->@@diff_start_end", {{&LogToFeaturesOptions::sojourn_time, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@arrival_rate", {{&LogToFeaturesOptions::diff_start_end, "@@arrival_rate"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@finish_rate", {{&LogToFeaturesOptions::diff_start_end, "@@finish_rate"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@service_time", {{&LogToFeaturesOptions::diff_start_end, "@@service_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@waiting_time", {{&LogToFeaturesOptions::diff_start_end, "@@waiting_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@sojourn_time", {{&LogToFeaturesOptions::diff_start_end, "@@sojourn_time"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|single:diff_start_end->@@diff_start_end", {{&LogToFeaturesOptions::diff_start_end, "@@diff_start_end"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:arrival_rate+finish_rate->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}}, {
  {1704067200, {4, 4, 6, 28.333333333333332, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:arrival_rate+service_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:arrival_rate+waiting_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:arrival_rate+sojourn_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {4, 4, 6, 30, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:arrival_rate+diff_start_end->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::diff_start_end, "shared"}}, {
  {1704067200, {4, 4, 6, 0, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:finish_rate+service_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:finish_rate+waiting_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:finish_rate+sojourn_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 30, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:finish_rate+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 0, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:service_time+waiting_time->shared", {{&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:service_time+sojourn_time->shared", {{&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 0, 30, 30, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:service_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:waiting_time+sojourn_time->shared", {{&LogToFeaturesOptions::sojourn_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:waiting_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|pair:sojourn_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {4, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|all->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {4, 4, 6, 0, 0, 0, 0, 0, 0, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:arrival_rate->org:resource", {{&LogToFeaturesOptions::arrival_rate, "org:resource"}}, {
  {1704067200, {3, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1}},
}},
{"D|category:arrival_rate->concept:name", {{&LogToFeaturesOptions::arrival_rate, "concept:name"}}, {
  {1704067200, {4, 3, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:finish_rate->org:resource", {{&LogToFeaturesOptions::finish_rate, "org:resource"}}, {
  {1704067200, {3, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1}},
}},
{"D|category:finish_rate->concept:name", {{&LogToFeaturesOptions::finish_rate, "concept:name"}}, {
  {1704067200, {4, 3, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:service_time->org:resource", {{&LogToFeaturesOptions::service_time, "org:resource"}}, {
  {1704067200, {1, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 3, 2, 3, 1}},
}},
{"D|category:service_time->concept:name", {{&LogToFeaturesOptions::service_time, "concept:name"}}, {
  {1704067200, {4, 1, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:waiting_time->org:resource", {{&LogToFeaturesOptions::waiting_time, "org:resource"}}, {
  {1704067200, {3, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1}},
}},
{"D|category:waiting_time->concept:name", {{&LogToFeaturesOptions::waiting_time, "concept:name"}}, {
  {1704067200, {4, 3, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:sojourn_time->org:resource", {{&LogToFeaturesOptions::sojourn_time, "org:resource"}}, {
  {1704067200, {3, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 1, 2, 3, 1}},
}},
{"D|category:sojourn_time->concept:name", {{&LogToFeaturesOptions::sojourn_time, "concept:name"}}, {
  {1704067200, {4, 3, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"D|category:diff_start_end->org:resource", {{&LogToFeaturesOptions::diff_start_end, "org:resource"}}, {
  {1704067200, {1, 4, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 0, 3, 2, 3, 1}},
}},
{"D|category:diff_start_end->concept:name", {{&LogToFeaturesOptions::diff_start_end, "concept:name"}}, {
  {1704067200, {4, 1, 6, 13.333333333333334, 28.333333333333332, 30, 30, 0, 3, 1, 2, 3, 1.3333333333333333}},
}},
{"30s|single:arrival_rate->@@arrival_rate", {{&LogToFeaturesOptions::arrival_rate, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:arrival_rate->@@finish_rate", {{&LogToFeaturesOptions::arrival_rate, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 70, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 70, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:arrival_rate->@@service_time", {{&LogToFeaturesOptions::arrival_rate, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:arrival_rate->@@waiting_time", {{&LogToFeaturesOptions::arrival_rate, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:arrival_rate->@@sojourn_time", {{&LogToFeaturesOptions::arrival_rate, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:arrival_rate->@@diff_start_end", {{&LogToFeaturesOptions::arrival_rate, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@arrival_rate", {{&LogToFeaturesOptions::finish_rate, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 70, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 70, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@finish_rate", {{&LogToFeaturesOptions::finish_rate, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@service_time", {{&LogToFeaturesOptions::finish_rate, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@waiting_time", {{&LogToFeaturesOptions::finish_rate, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@sojourn_time", {{&LogToFeaturesOptions::finish_rate, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:finish_rate->@@diff_start_end", {{&LogToFeaturesOptions::finish_rate, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@arrival_rate", {{&LogToFeaturesOptions::service_time, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@finish_rate", {{&LogToFeaturesOptions::service_time, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@service_time", {{&LogToFeaturesOptions::service_time, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@waiting_time", {{&LogToFeaturesOptions::service_time, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@sojourn_time", {{&LogToFeaturesOptions::service_time, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 0, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 0, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 0, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|single:service_time->@@diff_start_end", {{&LogToFeaturesOptions::service_time, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@arrival_rate", {{&LogToFeaturesOptions::waiting_time, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@finish_rate", {{&LogToFeaturesOptions::waiting_time, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@service_time", {{&LogToFeaturesOptions::waiting_time, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@waiting_time", {{&LogToFeaturesOptions::waiting_time, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@sojourn_time", {{&LogToFeaturesOptions::waiting_time, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:waiting_time->@@diff_start_end", {{&LogToFeaturesOptions::waiting_time, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@arrival_rate", {{&LogToFeaturesOptions::sojourn_time, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@finish_rate", {{&LogToFeaturesOptions::sojourn_time, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@service_time", {{&LogToFeaturesOptions::sojourn_time, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 0, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 0, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 0, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@waiting_time", {{&LogToFeaturesOptions::sojourn_time, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@sojourn_time", {{&LogToFeaturesOptions::sojourn_time, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:sojourn_time->@@diff_start_end", {{&LogToFeaturesOptions::sojourn_time, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@arrival_rate", {{&LogToFeaturesOptions::diff_start_end, "@@arrival_rate"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@finish_rate", {{&LogToFeaturesOptions::diff_start_end, "@@finish_rate"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@service_time", {{&LogToFeaturesOptions::diff_start_end, "@@service_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@waiting_time", {{&LogToFeaturesOptions::diff_start_end, "@@waiting_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@sojourn_time", {{&LogToFeaturesOptions::diff_start_end, "@@sojourn_time"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|single:diff_start_end->@@diff_start_end", {{&LogToFeaturesOptions::diff_start_end, "@@diff_start_end"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:arrival_rate+finish_rate->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 70, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 70, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:arrival_rate+service_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:arrival_rate+waiting_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:arrival_rate+sojourn_time->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {2, 3, 3, 15, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 60, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 60, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:arrival_rate+diff_start_end->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::diff_start_end, "shared"}}, {
  {1704067200, {2, 3, 3, 0, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:finish_rate+service_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:finish_rate+waiting_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:finish_rate+sojourn_time->shared", {{&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 15, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 60, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 60, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:finish_rate+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 0, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 0, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 0, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:service_time+waiting_time->shared", {{&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|pair:service_time+sojourn_time->shared", {{&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 0, 15, 15, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 0, 60, 60, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 0, 60, 60, 0, 1, 2, 1, 2}},
}},
{"30s|pair:service_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:waiting_time+sojourn_time->shared", {{&LogToFeaturesOptions::sojourn_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:waiting_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|pair:sojourn_time+diff_start_end->shared", {{&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 2}},
}},
{"30s|all->shared", {{&LogToFeaturesOptions::arrival_rate, "shared"}, {&LogToFeaturesOptions::diff_start_end, "shared"}, {&LogToFeaturesOptions::finish_rate, "shared"}, {&LogToFeaturesOptions::service_time, "shared"}, {&LogToFeaturesOptions::sojourn_time, "shared"}, {&LogToFeaturesOptions::waiting_time, "shared"}}, {
  {1704067200, {2, 3, 3, 0, 0, 0, 0, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 2, 2, 0, 0, 0, 0, 0, 0, 1, 2, 1, 2}},
}},
{"30s|category:arrival_rate->org:resource", {{&LogToFeaturesOptions::arrival_rate, "org:resource"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:arrival_rate->concept:name", {{&LogToFeaturesOptions::arrival_rate, "concept:name"}}, {
  {1704067200, {2, 2, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
{"30s|category:finish_rate->org:resource", {{&LogToFeaturesOptions::finish_rate, "org:resource"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:finish_rate->concept:name", {{&LogToFeaturesOptions::finish_rate, "concept:name"}}, {
  {1704067200, {2, 2, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
{"30s|category:service_time->org:resource", {{&LogToFeaturesOptions::service_time, "org:resource"}}, {
  {1704067200, {1, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 2, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:service_time->concept:name", {{&LogToFeaturesOptions::service_time, "concept:name"}}, {
  {1704067200, {2, 1, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
{"30s|category:waiting_time->org:resource", {{&LogToFeaturesOptions::waiting_time, "org:resource"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:waiting_time->concept:name", {{&LogToFeaturesOptions::waiting_time, "concept:name"}}, {
  {1704067200, {2, 2, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
{"30s|category:sojourn_time->org:resource", {{&LogToFeaturesOptions::sojourn_time, "org:resource"}}, {
  {1704067200, {2, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:sojourn_time->concept:name", {{&LogToFeaturesOptions::sojourn_time, "concept:name"}}, {
  {1704067200, {2, 2, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
{"30s|category:diff_start_end->org:resource", {{&LogToFeaturesOptions::diff_start_end, "org:resource"}}, {
  {1704067200, {1, 3, 3, 7.5, 7.5, 15, 15, 0, 0, 2, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {1, 2, 2, 25, 70, 60, 60, 0, 0, 1, 2, 1, 1}},
}},
{"30s|category:diff_start_end->concept:name", {{&LogToFeaturesOptions::diff_start_end, "concept:name"}}, {
  {1704067200, {2, 1, 3, 7.5, 7.5, 15, 15, 0, 1, 1, 1.5, 2, 1}},
  {1704067230, {1, 1, 1, 25, 70, 60, 60, 0, 0, 1, 1, 1, 1}},
  {1704067260, {2, 1, 2, 25, 70, 60, 60, 0, 1, 1, 2, 1, 2}},
}},
