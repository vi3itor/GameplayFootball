// Match reporting adapted from Tijmen Krijnen's 2022 GameplayFootball PASS export.
// Modified for optional export, correct event attribution and safe file handling.
// Typed JSON follows Lorenzo Gatti's PASS exporter change (fc5c6bf).
// Licensed under the Apache License, Version 2.0; see LICENSE.
#include "match_report.hpp"

#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace match_report {
namespace {
void PutPerson(Record &record, const Person &person) {
  const std::string first = Utf8(person.first_name), last = Utf8(person.last_name);
  record["c_Person"] = first.empty() ? last : last.empty() ? first : first + " " + last;
  record["c_PersonFirstName"] = first;
  record["c_PersonLastName"] = last;
  record["c_PersonShort"] = last;
  record["c_PersonSort"] = last.empty() ? first : first.empty() ? last : last + ", " + first;
  record["c_Team"] = Utf8(person.team);
  record["c_TeamShort"] = Utf8(person.team_short);
}

std::string Date(std::int64_t ms) { return "/Date(" + std::to_string(ms) + ")/"; }

} // namespace

std::int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string Utf8(const std::string &text) {
  std::string result;
  for (std::size_t i = 0; i < text.size();) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    std::size_t size = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 :
      c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
    bool valid = size != 0 && i + size <= text.size();
    for (std::size_t j = 1; valid && j < size; ++j) {
      const unsigned char next = static_cast<unsigned char>(text[i + j]);
      valid = next >= 0x80 && next <= 0xbf;
    }
    if (valid && size >= 3) {
      const unsigned char next = static_cast<unsigned char>(text[i + 1]);
      valid = !(c == 0xe0 && next < 0xa0) && !(c == 0xed && next >= 0xa0) &&
        !(c == 0xf0 && next < 0x90) && !(c == 0xf4 && next >= 0x90);
    }
    if (valid) {
      result.append(text, i, size);
      i += size;
    } else {
      result += "\xef\xbf\xbd";
      ++i;
    }
  }
  return result;
}

Record MakeAction(Action action, const Person &person, int period,
                  unsigned long time_ms, int home_goals, int away_goals,
                  int beneficiary) {
  const char *label = "";
  std::array<int, 4> codes{{0, 0, 0, 0}};
  switch (action) {
    case Action::kick_off: label = "Aftrap"; codes = {{0, 2097152, 1024, 0}}; break;
    case Action::goal_kick: label = "Doeltrap"; codes = {{0, 2097152, 2048, 0}}; break;
    case Action::free_kick: label = "Vrije Trap"; codes = {{0, 2097152, 256, 0}}; break;
    case Action::corner: label = "Hoekschop"; codes = {{0, 2097152, 128, 0}}; break;
    case Action::penalty: label = "Strafschop"; codes = {{0, 2097152, 64, 0}}; break;
    case Action::throw_in: label = "Inworp"; codes = {{0, 2097152, 512, 0}}; break;
    case Action::goal: label = "Doelpunt"; codes = {{4, 0, 0, 1}}; break;
    case Action::own_goal: label = "Eigen doelpunt"; codes = {{64, 0, 0, 1}}; break;
    case Action::offside: label = "Buitenspel"; codes = {{262144, 0, 0, 12}}; break;
    case Action::foul_free_kick: label = "Overtreding (vrije trap)"; codes = {{2097152, 0, 0, 12}}; break;
    case Action::foul_penalty: label = "Overtreding (strafschop)"; codes = {{2097152, 0, 0, 12}}; break;
    case Action::yellow_card: label = "Geel"; codes = {{2048, 0, 0, 3}}; break;
    case Action::red_card: label = "Rood"; codes = {{8192, 0, 0, 3}}; break;
    case Action::second_yellow: label = "Rood (2x geel)"; codes = {{4096, 0, 0, 3}}; break;
  }
  Record record;
  PutPerson(record, person);
  record["c_Action"] = label;
  record["c_ActionMinute"] = std::to_string(time_ms / 60000);
  record["c_Period"] = period;
  record["n_ActionCode"] = codes[0];
  record["n_ActionCode2"] = codes[1];
  record["n_ActionCode3"] = codes[2];
  record["n_ActionSet"] = codes[3];
  record["n_ActionTime"] = time_ms;
  record["n_HomeGoals"] = home_goals;
  record["n_AwayGoals"] = away_goals;
  record["n_HomeOrAway"] = person.home_or_away;
  record["n_BeneficiaryHomeOrAway"] = beneficiary;
  return record;
}

Record MakeLineup(const Person &person, Role role, bool sent_off, bool starter) {
  Record record;
  PutPerson(record, person);
  const char *name = "undefined", *short_name = "?";
  int code = 0;
  switch (role) {
    case Role::goalkeeper: name = "Keeper"; short_name = "K"; code = 1; break;
    case Role::defender: name = "Verdediger"; short_name = "V"; code = 2; break;
    case Role::midfielder: name = "Middenvelder"; short_name = "M"; code = 4; break;
    case Role::attacker: name = "Aanvaller"; short_name = "A"; code = 8; break;
    case Role::unknown: break;
  }
  record["b_RedCard"] = sent_off;
  record["b_Starter"] = starter;
  record["c_Function"] = name;
  record["c_FunctionShort"] = short_name;
  record["n_FunctionCode"] = code;
  return record;
}

Record MakeInfo(const Info &info) {
  Record record;
  record["c_HomeTeam"] = Utf8(info.home_team);
  record["c_HomeTeamShort"] = Utf8(info.home_short);
  record["c_AwayTeam"] = Utf8(info.away_team);
  record["c_AwayTeamShort"] = Utf8(info.away_short);
  std::string score = std::to_string(info.goals[0]) + "-" + std::to_string(info.goals[1]);
  if (info.halftime_known) score += " (" + std::to_string(info.halftime_goals[0]) + "-" +
    std::to_string(info.halftime_goals[1]) + ")";
  record["c_Score"] = score;
  record["b_HalfTimeKnown"] = info.halftime_known;
  record["c_Status"] = info.completed ? "completed" : "abandoned";
  record["d_Date"] = Date(info.end_ms);
  record["d_DateLocal"] = Date(info.end_ms);
  record["d_MatchStartTime"] = Date(info.start_ms);
  record["d_MatchStartTimeLocal"] = Date(info.start_ms);
  record["n_HomeGoals"] = info.goals[0];
  record["n_AwayGoals"] = info.goals[1];
  record["n_Spectators"] = 0;
  return record;
}

std::string Serialize(const Records &actions, const Record &info, const Records &lineup) {
  return Record{
    {"MatchActions", actions},
    {"MatchInfo", Record::array({info})},
    {"MatchLineup", lineup}
  }.dump() + '\n';
}

std::string Write(const std::string &directory, std::int64_t start_ms, const std::string &json) {
  namespace fs = boost::filesystem;
  if (directory.empty()) throw std::invalid_argument("Match report directory is empty");
  const fs::path dir(directory);
  fs::create_directories(dir);
  // An exclusive directory reservation prevents concurrent matches overwriting files.
  fs::path reservation;
  do {
    reservation = dir / fs::unique_path("match-" + std::to_string(start_ms) + "-%%%%-%%%%-%%%%");
  } while (!fs::create_directory(reservation));
  const fs::path temporary = reservation / "report.tmp";
  const fs::path destination = reservation / "report.json";
  try {
    std::ofstream stream(temporary.string(), std::ios::binary);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream << json;
    stream.close();
    fs::rename(temporary, destination);
  } catch (...) {
    boost::system::error_code ignored;
    fs::remove(temporary, ignored);
    fs::remove(reservation, ignored);
    throw;
  }
  return destination.string();
}
} // namespace match_report
