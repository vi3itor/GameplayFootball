// Regression coverage for the adapted Tijmen Krijnen PASS report exporter.
#include "utils/match_report.hpp"
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace {
void Check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

match_report::Record Parse(const std::string &json) {
  std::istringstream stream(json);
  match_report::Record record;
  boost::property_tree::read_json(stream, record);
  return record;
}

std::string Read(const boost::filesystem::path &path) {
  std::ifstream stream(path.string(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string TestRecords() {
  using namespace match_report;
  Person home, away;
  home.first_name = "A\"lex\n";
  home.last_name = "Zo\xc3\xab";
  home.team = "Home";
  home.team_short = "HOM";
  home.home_or_away = 0;
  away.first_name = "Visitor";
  away.last_name = "Player";
  away.team = "Away";
  away.team_short = "AWY";
  away.home_or_away = 1;
  Records actions;
  actions.push_back(MakeAction(Action::kick_off, home, 1, 0, 0, 0));
  actions.push_back(MakeAction(Action::goal, home, 1, 60000, 1, 0, 0));
  actions.push_back(MakeAction(Action::own_goal, away, 1, 120000, 2, 0, 0));
  actions.push_back(MakeAction(Action::offside, away, 1, 130000, 2, 0, 0));
  Check(actions[1].get<int>("n_ActionCode") == 4, "goal code changed");
  Check(actions[2].get<int>("n_ActionCode") == 64, "own-goal code changed");
  Check(actions[2].get<int>("n_HomeOrAway") == 1 &&
        actions[2].get<int>("n_BeneficiaryHomeOrAway") == 0, "own-goal attribution lost");
  Check(actions[3].get<std::string>("c_Team") == "Away" &&
        actions[3].get<int>("n_HomeOrAway") == 1, "offside actor assigned opposing team");
  Check(actions[3].get<int>("n_BeneficiaryHomeOrAway") == 0, "offside beneficiary lost");
  Person unknown;
  unknown.team = "Home";
  unknown.home_or_away = 0;
  const auto unknown_goal = MakeAction(Action::goal, unknown, 1, 0, 1, 0, 0);
  Check(unknown_goal.get<std::string>("c_Person").empty() &&
        unknown_goal.get<std::string>("c_Team") == "Home", "unknown scorer/team lost");

  auto red = MakeAction(Action::red_card, home, 2, 3000000, 2, 0);
  auto second = MakeAction(Action::second_yellow, home, 2, 3000010, 2, 0);
  Check(red.get<int>("n_ActionCode") == 8192 && second.get<int>("n_ActionCode") == 4096,
        "direct red and second yellow must be distinct");
  Check(red.get<std::string>("c_Action") == "Rood", "direct red mislabeled");
  Check(MakeAction(Action::yellow_card, home, 1, 0, 0, 0).get<int>("n_ActionCode") == 2048,
        "yellow-card code changed");
  const Action restarts[] = {Action::goal_kick, Action::free_kick, Action::corner,
    Action::penalty, Action::throw_in};
  const int restart_codes[] = {2048, 256, 128, 64, 512};
  for (int i = 0; i < 5; ++i) {
    auto record = MakeAction(restarts[i], home, 1, 0, 0, 0);
    Check(record.get<int>("n_ActionCode3") == restart_codes[i], "set-piece code changed");
  }
  Check(MakeAction(Action::foul_penalty, home, 1, 0, 0, 0).get<int>("n_ActionSet") == 12,
        "foul action set changed");
  Check(MakeAction(Action::foul_free_kick, home, 1, 0, 0, 0).get<int>("n_ActionCode") == 2097152,
        "free-kick foul code changed");

  Records lineup{MakeLineup(home, Role::goalkeeper, true, true),
    MakeLineup(away, Role::unknown, false, false)};
  Check(lineup[0].get<int>("n_FunctionCode") == 1 &&
        lineup[0].get<bool>("b_RedCard"), "dismissed starter missing from lineup");
  Check(lineup[1].get<int>("n_FunctionCode") == 0 &&
        !lineup[1].get<bool>("b_Starter"), "bench/unknown role must be defined");
  Check(MakeLineup(home, Role::defender, false, true).get<int>("n_FunctionCode") == 2 &&
        MakeLineup(home, Role::midfielder, false, true).get<int>("n_FunctionCode") == 4 &&
        MakeLineup(home, Role::attacker, false, true).get<int>("n_FunctionCode") == 8,
        "PASS role codes changed");

  Info info;
  info.home_team = "Home";
  info.away_team = "Away";
  info.goals = {{2, 0}};
  info.start_ms = 1700000000123;
  info.end_ms = 1700000300123;
  auto abandoned = MakeInfo(info);
  Check(abandoned.get<std::string>("c_Status") == "abandoned" &&
        !abandoned.get<bool>("b_HalfTimeKnown"), "abandoned match presented as completed");
  Check(abandoned.get<std::string>("c_Score") == "2-0", "unknown halftime fabricated");
  const auto empty = Serialize({}, abandoned, {});
  Check(empty.find("\"MatchActions\":[]") != std::string::npos &&
        empty.find("\"MatchLineup\":[]") != std::string::npos, "empty collections must be JSON arrays");
  Parse(empty);
  info.completed = true;
  info.halftime_known = true;
  info.halftime_goals = {{1, 0}};
  const auto json = Serialize(actions, MakeInfo(info), lineup);
  const auto root = Parse(json);
  Check(root.get_child("MatchActions").size() == 4 && root.get_child("MatchLineup").size() == 2,
        "records lost during JSON serialization");
  const auto &read_info = root.get_child("MatchInfo").front().second;
  Check(read_info.get<std::string>("c_Score") == "2-0 (1-0)", "halftime score lost");
  Check(read_info.get<std::string>("d_MatchStartTime") == "/Date(1700000000123)/",
        "timestamp truncated on Windows");
  Check(root.get_child("MatchActions").front().second.get<std::string>("c_PersonFirstName") == home.first_name,
        "JSON escaping corrupted name");
  Check(json.find("\"n_HomeGoals\":\"2\"") != std::string::npos,
        "PASS string scalar compatibility changed");
  return json;
}

void TestEncoding() {
  using match_report::Utf8;
  const std::string valid = "Zo\xc3\xab \xe2\x82\xac \xf0\x9f\x98\x80";
  Check(Utf8(valid) == valid, "valid UTF-8 changed");
  Check(Utf8(std::string("\xe9", 1)) == "\xef\xbf\xbd", "legacy invalid byte must be replaced");
  const std::string invalid[] = {"\xc0\xaf", "\xe0\x80\xaf", "\xed\xa0\x80",
    "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xe2\x82", "\x80"};
  for (const auto &text : invalid) Check(Utf8(Utf8(text)) == Utf8(text), "invalid UTF-8 survived repair");
}

void TestFiles(const std::string &json) {
  namespace fs = boost::filesystem;
  const fs::path root = fs::temp_directory_path() / fs::unique_path("gameplayfootball-report-test-%%%%-%%%%");
  fs::create_directory(root);
  try {
    const auto a = match_report::Write((root / "reports").string(), 42, json);
    const auto b = match_report::Write((root / "reports").string(), 42, "second match\n");
    Check(a != b && Read(a) == json && Read(b) == "second match\n", "repeated matches overwrite reports");
    bool failed = false;
    try { match_report::Write((fs::path(a) / "not-a-directory").string(), 42, json); }
    catch (const std::exception &) { failed = true; }
    Check(failed && Read(a) == json, "invalid output path not detected or damaged earlier report");
    Check(!fs::exists(fs::path(a).parent_path() / "report.tmp"), "partial file left after success");
    failed = false;
    try { match_report::Write("", 42, json); } catch (const std::exception &) { failed = true; }
    Check(failed, "empty output directory must be rejected");
  } catch (...) { fs::remove_all(root); throw; }
  fs::remove_all(root);
}
} // namespace

int main(int argc, char **argv) {
  try {
    const auto example = TestRecords();
    TestEncoding();
    TestFiles(example);
    if (argc == 3 && std::string(argv[1]) == "--example") {
      std::ofstream stream(argv[2], std::ios::binary);
      stream.exceptions(std::ios::badbit | std::ios::failbit);
      boost::property_tree::write_json(stream, Parse(example), true);
      stream.close();
    }
    std::cout << "Match report regression checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
