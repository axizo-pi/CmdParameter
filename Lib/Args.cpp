#include "Args.h"
#include "Support/Exception.h"
#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

using namespace std;

namespace {

bool contains(string const &s, char c) {
  return (s.find(c) != std::string::npos);
}

}  // anon namespace


/**
 * @brief Read in command line arguments.
 */
Args::Args(int argc, char const *argv[]) {
  for (int i = 1; i < argc; ++i) {  // Skip first param, which is the program name
    //cout << "argv[i]: " << argv[i] << endl;
    push_back(argv[i]);
  }
}


/**
 * @brief Split the string into its constituent one-letter parameters.
 */
Args::Args(std::string const &w) {
  // Collect the combined arguments
  for (int i = 1; i < (int) w.length(); ++i) {            // Skip first, this is '-'
    string arg("-");
    char c = w[i];
    arg += c;
    push_back(arg);
  }
}


/**
 * @brief check if a single parameter is valid.
 *
 * @return true if valid, false otherwise
 */
bool Args::check(List const &parameters, std::string const &w) const {
  if (is_help(w)) return true;

  for (auto &item: parameters) {
    TypedParameter &param = *item;

    if (param.match_param(w)) {
      //cout << "check() w checks out: " << w << endl;
      return true;
    }
  }

  return false;
}


/**
 * @brief check parameter matches starting from index `current`.
 *
 * This method is intended to test action parameters.
 *
 * @param current index of first args to check
 * @return        number of parameters that match
 */
int Args::check(List const &parameters, int current) const {
  int ret = 0;

  for (int i = current; i < (int) size(); ++i) {
    auto const &w = (*this)[i];

    // Quit if no (more) matches
    // This can go wrong if an action parameter is the same as a global parameter
    if (!check(parameters, w)) break; 

    ++ret;
  }

  return ret;
}


/**
 * @brief Append List rhs to current.
 */
Args &Args::operator<<(Args const &rhs) {
  (*this).insert((*this).end(), rhs.begin(), rhs.end());
  return *this;
}


/**
 * @brief Prescan contained parameters without handling.
 *
 * **ALL** values should match.
 *
 * Combined parameters are split and tested separately.
 * If they _all_ check out, these are added to the list,
 * replacing the current list.
 *
 * @param  parameters Parameter definitions to check against.
 * @return            true if all parameters check out, false otherwise.
 *
 * =========================================================
 * Notes
 * -----
 *
 * 1. A parameter might be a combined sequence of single-letter param's.
 *    They are tested separately, **ALL** should succeed.
 *
 *    Technically, the final param _could_ take a value. We won't handle this.
 */
bool Args::check(List const &parameters, DefActions const &actions, bool do_throw) {
  if (empty()) return true; // This needs to be adjusted if required parameters are implemented

  auto get_action = [&actions] (std::string const &w) -> DefAction const * {
    for (auto const &a : actions) {
      if (w == a.name) return &a;
    }

    return nullptr;
  };

  int unnamed_count = 0;
	std::string err_msg;

  auto unnamed_present = [&parameters] () -> bool {
    int count = 0;

    for (auto const &ptr : parameters) {
      auto &p = *ptr;

      if (p.def_param.param_type == UNNAMED) {
        ++count;
      }
    }

    assert(count <= 1);  // Not expecting more than 1 UNNAMED definition
    return count > 0;
  };


  Args new_list;
  bool checks_out = true;

  for (int current = 0; current < (int) size(); ++current) {
    auto &w = (*this)[current];

    auto p_action = get_action(w);
    if (p_action != nullptr) {
/*
      cout << "Args::check detected action '" << p_action->name << "', "
           << p_action->parameters.size() << " parameters."
           << endl;
*/
      new_list.push_back(w);

      int num_params = check(p_action->parameters, current + 1);
      if (num_params > 0) {
        for (int i = 0; i < num_params; ++i ) {
          auto &w = (*this)[i + current + 1];
          new_list.push_back(w);
        }

        current += num_params;
      }

      continue;
    } else if (check(parameters, w)) {
      new_list.push_back(w);
      continue;
    } else if (unnamed_present()) {
      // Assume parameter is unnamed
      unnamed_count++;
      new_list.push_back(w);
      continue;
    }

    bool do_combined = (w[0] == '-' && w.length() > 2 && !contains(w, '='));

    if (!do_combined) {
      if (do_throw) {
				if (!err_msg.empty()) err_msg += "  ";
        err_msg += "Parameter '" + w + "' not defined.\n";
      }

      new_list.push_back(w);
			checks_out = false;
      continue;
    }

    //
    // Check combines of single-character options. See Note 1.
    //
    Args w_tmp(w);
    //cout << "check combined args: " << w_tmp.dump();

    if (w_tmp.check(parameters, actions, false)) {
      new_list << w_tmp;
    } else {
      err_msg += "Parameter '" + w + "' not defined. This might be a combined parameter.\n";
      new_list.push_back(w);
    }
  }

  if (unnamed_count > 1) {
    err_msg += "Multiple unnamed parameters, expecting at most 1.\n";
  }

  *this = new_list;

	if (!err_msg.empty()) {
		throw Exception(err_msg);
		checks_out = false;
	}

  return checks_out;
}


/**
 * @brief Handle the command line parameters
 *
 * The handling results are store in parameter `parameters.
 *
 * @return true if handling succeeded, false if any parameter handling failed.
 */
bool Args::handle(List &parameters) {
  bool ret = true;

  for (auto &w2: (*this)) {
    bool did_it = false;

    for (auto &item: parameters) {
      TypedParameter &param = *item;

      if (param.parse_param(w2)) {                        // Processing could fail, even if param itself is legal
        did_it = true;
        break;
      }
    }

    if (!did_it) ret = false;                             // Continue loop to catch any other errors
  }

  return ret;
}


bool Args::is_help(std::string const &w) const {
  return (w == "help" || w == "-h");
}


/**
 * @brief Scan for help switch
 *
 * Note that -h combined with other single-letter params gets missed
 *
 * @return true if help switch found, false otherwise
 */
bool Args::has_help() const {
  for (auto w: (*this)) {
    if (is_help(w))  return true;
  }

  return false;
}


std::string Args::dump() {
  std::stringstream ret;

  for (auto const &w: (*this)) {
    ret << w << ", ";
  }

  ret << "\n";
  return ret.str();
}
