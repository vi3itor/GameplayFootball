// Match reporting adapted from Tijmen Krijnen's 2022 GameplayFootball PASS export.
// Modified for optional export, correct event attribution and safe file handling.
// Licensed under the Apache License, Version 2.0; see LICENSE.
#ifndef GAMEPLAYFOOTBALL_MATCH_REPORT_HPP
#define GAMEPLAYFOOTBALL_MATCH_REPORT_HPP

#include <boost/property_tree/ptree.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace match_report {

enum class Action {
  kick_off, goal_kick, free_kick, corner, penalty, throw_in,
  goal, own_goal, offside, foul_free_kick, foul_penalty,
  yellow_card, red_card, second_yellow
};

enum class Role { unknown, goalkeeper, defender, midfielder, attacker };

struct Person {
  std::string first_name, last_name, team, team_short;
  int home_or_away = -1;
};

struct Info {
  std::string home_team, home_short, away_team, away_short;
  std::array<int, 2> goals{{0, 0}}, halftime_goals{{0, 0}};
  bool halftime_known = false;
  bool completed = false;
  std::int64_t start_ms = 0, end_ms = 0;
};

using Record = boost::property_tree::ptree;
using Records = std::vector<Record>;

std::int64_t NowMs();
// Preserve UTF-8; replace invalid source bytes with U+FFFD, without changing the DB.
std::string Utf8(const std::string &text);
Record MakeAction(Action action, const Person &person, int period,
                  unsigned long time_ms, int home_goals, int away_goals,
                  int beneficiary = -1);
Record MakeLineup(const Person &person, Role role, bool sent_off, bool starter);
Record MakeInfo(const Info &info);
// PASS uses string scalars. Write array delimiters explicitly, including [].
std::string Serialize(const Records &actions, const Record &info, const Records &lineup);
// Creates a unique file; throws on failure. Callers must not let export stop cleanup.
std::string Write(const std::string &directory, std::int64_t start_ms,
                  const std::string &json);

} // namespace match_report
#endif
