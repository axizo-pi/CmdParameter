#include "CmdParameters.h"
#include <cassert>
#include <iostream>
#include <cstring>   // strcmp()
#include <functional>
#include "Support/Exception.h"
#include "Support/basics.h"
#include "Types/Types.h"
#include "lib_local.h"

using namespace std;

namespace {

enum {
  PAD_OFFSET = 5
};

using List = TypedParameter::List;

string pad(unsigned width, std::string const &str = "") {
  string out;

  for (auto i = str.length(); i < width; ++ i) {
    out += " ";
  }

  return str + out;
}


bool contains(string const &s, char c) {
  return (s.find(c) != std::string::npos);
}


/**
 * **Pre:** The combined options are split.
 */
bool process_option(List &parameters, string const &w) {
  for (auto &item: parameters) {
    TypedParameter &param = *item;
    if (param.parse_param(w)) return true;
  }

  return false;
}


string set_indent(int indent, string const &str) {
  string str_indent("\n");
  str_indent += pad(indent);
  return Strings::implode(Strings::explode(str, '\n'), str_indent.c_str());
}


/**
 * @brief determine longest string length in string vector.
 */
unsigned max_width(StrList const &list) {
  unsigned width = 0;

  for (const string &str : list) {
    // CRAP: Following fails when 'int width = -1', due to signedness
    if (str.length() > width) {
      width = (unsigned) str.length();
    }
  }

  return width;
}


std::string get_just_params(TypedParameter::List const &parameters, bool add_help) {
  std::string ret;

  TypedParameter::SortedList sorted_list(parameters);
  StrList disp_defaults = sorted_list.defaults_str(add_help);
  StrList disp_params   = sorted_list.prefixes_str(add_help);

  unsigned width = max_width(disp_params);

  auto output_padded = [&ret, width] (
    TypedParameter const &param,
    string const &disp_param,
    string const &disp_default) {
    // Ensure line endings have proper indent
    ret << "   " << pad(width, disp_param) << "  " << set_indent(width + PAD_OFFSET, param.usage())
        << disp_default << "\n";
  };

  int index = 0;

  if (add_help) {
    output_padded(CmdParameters::help_switch, disp_params[index], disp_defaults[index]);
    index++;
  }


  for (auto &item: sorted_list) {
    TypedParameter const &param = *item;

    output_padded(param, disp_params[index], disp_defaults[index]);
    index++;
  }

  return ret;
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
        err_msg += "  Parameter '" + w + "' not defined.\n";
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

	if (!err_msg.empty()) {
		throw Exception(err_msg);
		checks_out = false;
	}

  if (checks_out) {
    *this = new_list;
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


// Internal definition of help switch
DefParameter CmdParameters::help_def(
  "Help switch",
  { "help", "-h" },
  NONE,
  "Show this information. Overrides all other parameters"
);


NoneParameter CmdParameters::help_switch(CmdParameters::help_def);

CmdParameters::CmdParameters(CmdParameters const &rhs) : m_description(rhs.m_description) {
  init(&rhs);
}


CmdParameters::CmdParameters(
  std::string const &in_description,
  DefParameters global_params,
  CmdParameters *parent
) :
  m_description(in_description),
  global_parameters(global_params)
{
  init(parent);
}


CmdParameters::CmdParameters(
  std::string const &in_description,
  DefActions in_actions,
  CmdParameters *parent
) :
  m_description(in_description),
  actions(in_actions)
{
  init(parent);
}


CmdParameters::CmdParameters(
  std::string const &in_description,
  DefActions in_actions,
  DefParameters global_params,
  CmdParameters *parent) :
  m_description(in_description),
  global_parameters(global_params),
  actions(in_actions)
{
  init(parent);
}


/**
 * @brief Create help usage text for all the defined global parameters.
 *
 * Output per option has the form:
 *
 *   switch[=<param_type>] description[<text for default>]
 *
 * Some effort is done to keep the descriptions aligned.
 */
std::string CmdParameters::get_usage() const {
  std::string ret;

  ret << m_description
      << get_actions()
      << get_params(m_parameters);

  return ret;
}


void CmdParameters::output_errors(bool show_help_on_error) const {
  if (!has_errors()) return;
  if (m_silent) return; 

  cout << "Error(s) on command line:\n" << get_errors() << endl;

  if (show_help_on_error) {
    cout << get_usage();
  } else {
    cout << "  Use 'help' or '-h' to view options\n"  << endl;
  }
}


/**
 * @brief Override of `handle_commandline()` which also does `init()`.
 *
 * @return ALL_IS_WELL   if all is well,
 *         EXIT_NO_ERROR if should stop without errors,
 *         EXIT_ERROR    if should stop with errors
 */
CmdParameters::ExitCode CmdParameters::handle_commandline(int argc, char const *argv[], bool show_help_on_error) {
  Args args(argc, argv);

  if (init()) {
    reset();

    handle_commandline_intern(args);

    if (args.has_help()) {
			cout << "has_help" << endl;

      // Ignore all errors when help flag detected
      show_help(args);
      return EXIT_NO_ERROR;
    }
  }

  //cout << "args2: " << args.dump();

  if (has_errors()) {
    output_errors(show_help_on_error);
    return EXIT_ERROR;
  } else {
    return ALL_IS_WELL;
  }
}


void CmdParameters::show_errors(bool do_explicit) {
  if (has_errors()) {
    if (do_explicit || !m_silent) {
      cout << "Error(s) on command line:\n" << get_errors() << endl;
    }
  } else {
    if (do_explicit) {
      cout << "  No errors encountered\n"  << endl;
    }
  }
}


void CmdParameters::reset() {
  errors.str("");
  errors.clear();
  m_p_action = nullptr;
  m_parameters.reset_values();
}


/**
 * Display help output
 */
void CmdParameters::show_help(Args const &args) {
  if (scan_action(args)) {
    cout << get_action_usage();
  } else {
    cout << get_usage();
  }
}


void CmdParameters::process_arg(char const *curarg) {
  assert(curarg != nullptr);

  // Global options first
  if (process_option(m_parameters, curarg)) return;

  // Actions
  if (handle_action(curarg, &errors)) return;
  if (handle_action_option(curarg)) return;

  // It's not an option or an action, so it must be unnamed input
  // NOTE: this implementation checks UNNAMED on global options only
  if (!m_parameters.process_unnamed(curarg)) {
    errors << "  Unknown parameter '" << curarg << "', "
           << "or too many unnamed parameters on command line.\n";
  }
}


/**
 * @brief Handle the command line and initialize files/dir's.
 *
 * @param args  Container for command line parameters.
 *              param gets changed if there are combined parameters.
 */
void CmdParameters::handle_commandline_intern(Args &args) {
  try {
    args.check(m_parameters, actions);

    for (auto w: args) {
      process_arg(w.c_str());
    }

    check_unnamed_fields();
    check_action_detected();
  } catch (Exception &e) {
    errors << "  " << e.what() << endl;
  }
}


bool CmdParameters::init(CmdParameters const *parent) {
  if (m_done_init) return m_init_result;  // already initialized

  errors.str("");
  errors.clear();
  m_validated = false;

  if (parent != nullptr) {
    if (!add_intern(*parent)) {
      m_init_result = false;
      return false;
    }
  }

  m_init_result = validate() && init_params() && init_actions();

  m_done_init = true;                     // This does not mean that init() was successful!
  return m_init_result;
}


bool CmdParameters::add_intern(CmdParameters const &rhs) {
  // rhs should have been init'ed already (in ctor)

  if (rhs.has_errors()) {
     errors << "Added instance has errors on init()\n"
            <<  rhs.get_errors();

    return false;
  }

  if (m_description.empty()) {
    m_description = rhs.m_description;
  }

  // Add the global param's of the parent to the local list.
  // This means that some double work is done. I don't care
   for(auto &item : rhs.global_parameters) {
    global_parameters.push_back(item);
   }

  // Add parent actions to list of this instance
  // Init on these has already been called
  // TODO: check if this is an issue
   for(auto &action : rhs.actions) {
    actions.push_back(action);
  }

  return true;
}


bool CmdParameters::add(CmdParameters const &rhs) {
  m_done_init = false;  // Need to redo init

  if (!add_intern(rhs)) return false;
  return init();
}


/**
 * @brief Process the usage definitions internally
 *
 * The parameter definitions are converted to an internal representation, better
 * suited for parsing the actual values on the command line.
 *
 * @return true if all went well, false if an error occured during conversion.
 */
bool CmdParameters::init_params() {
  m_parameters.clear();

  for(auto &item : global_parameters) {
    TypedParameter *p = DefParameter_factory(item);
    if (p == nullptr) return false;

    m_parameters.emplace_back(p);
   }
 
  return true;
}


bool CmdParameters::validate() {
  if (m_validated) return true;

  if (m_description.empty()) {
    m_validation.add_error("No description passed");
  }

  m_validated = m_validation.validate(global_parameters, actions);
  errors << m_validation.get_messages(); 

  return m_validated;
}


std::string CmdParameters::params_usage(bool add_help) const {
 return get_just_params(m_parameters, add_help);
}


std::string CmdParameters::get_params(TypedParameter::List const &parameters) const {
  std::string ret;

  bool have_actions = !actions.empty();

  if (have_actions) {
    ret += "\nGlobal Options:\n";
  } else {
    ret += "\nOptions:\n";
  }

 ret += params_usage(true);

  if (have_actions) {
    ret += "\nNotes:\n\n"
           " * Global options can appear in any position on the command line after the program name.\n"
           " * 'help' combined with an action shows the help for that action.\n"
           " * Actions-specific options must come *after* the action on the commandline.\n";
  }
  ret +=  "\n";

  return ret;
}


////////////////////////
// Action handling
////////////////////////

bool CmdParameters::init_actions() {
  for(auto &action : actions) {
    if (!action.init_params()) return false;
  }

  return true;
}


/**
 * @brief Check if passed string is an action.
 *
 * If valid, store locally.
 * Presence of multiple valid actions is regarded as an error.
 *
 * @return true if action, false otherwise
 */
bool CmdParameters::handle_action(char const *curarg, Buf *errors) {
  bool found_action = false;

  for (auto &action : actions) {
    if (strcmp(action.name, curarg) == 0) {
      found_action = true;

      if (m_p_action != nullptr) {
        if (errors != nullptr) {
          *errors << "Multiple actions encountered on the command line.\n";
        }
        break;
      }

      m_p_action = &action;
      //cout << "Found action '" << m_p_action->name << "'." << endl;  // DEBUG
    }
  }

  return found_action;
}


bool CmdParameters::handle_action_option(char const *curarg) {
  if (m_p_action == nullptr) return false; 
  return process_option(m_p_action->parameters, curarg);
}


void CmdParameters::check_unnamed_fields() {
  // Check if all unnamed fields have a value
  for (auto &ptr : m_parameters) {
    auto &field = *ptr.get();
    if (field.def_param.param_type != UNNAMED) continue;

    if (field.get_string_value().empty()) {
      errors << "  No " << field.def_param.name << " specified.\n";
    }
  }
}


void CmdParameters::check_action_detected() {
  // If actions defined, an action must have been detected
  if (!actions.empty() && m_p_action == nullptr) {
    // Collect all action names
    std::string names;

    for (auto & action : actions) {
      if (!names.empty()) {
        names += " ";
      }

      names += action.name;
    }

    errors << "  Action expected, use one of: " << names << "\n";
  }
}


/**
 * Scan command line for presence of actions
 *
 * @return true if (single) action found, false otherwise
 */
bool CmdParameters::scan_action(Args const &args) {
  if(m_p_action != nullptr) {
    assert(false);
    std::cout << "BOINK!" << std::endl;
    return true;
  }

  for (auto w: args) {
    if (this->handle_action(w.c_str())) break;  // Will set m_p_action if found
  }


  return (m_p_action != nullptr);
}


/**
 * Collect all action names in a list
 */
StrList CmdParameters::action_names() const {
  StrList disp_names;
  for (auto &action: actions) {
    disp_names.push_back(action.name);
  }

  return disp_names;
}


std::string CmdParameters::get_actions() const {
  if (actions.empty()) return "";

  unsigned width = max_width(action_names());

  std::string ret;
  ret << "\n\nActions:\n\n";

  for (auto &action: actions) {
    ret << "    " << pad(width, action.name) << "   " << set_indent(width + PAD_OFFSET, action.usage) << "\n";
  }

 return ret;
}


std::string CmdParameters::get_action_usage() const {
  std::string ret;

  assert(m_p_action != nullptr);
  auto p = m_p_action;

  ret << "\nHelp for action '" << p->name << "'\n\n"
      << "Description:\n\n"
      << "  " << set_indent(2, p->usage) << ".\n";

  if (p->long_usage != nullptr) {
    ret << "  " << set_indent(2, p->long_usage) << "\n";
  }

  ret << "\nOptions:\n"
      << get_just_params(p->parameters, false) << "\n";

  return ret;
}
