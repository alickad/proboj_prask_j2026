import sys
import os
import subprocess

IS_OS_WINDOWS = os.name == 'nt'
ENABLE_COLORS = not IS_OS_WINDOWS

ANSI_ESCAPE_SEQUENCES = {
    'bold': '\033[1m',
    'italic': '\033[3m',
    'red': '\033[31m',
    'green': '\033[32m',
    'yellow': '\033[33m',
    'blue': '\033[34m'
} 
ANSI_ESCAPE_SEQUENCE_END = '\033[0m'
# Small function to enable colorful text
def log(*msg, color=None):
    if not ENABLE_COLORS: color = None
    if isinstance(color, str): color = [color]
    if color:
        print(f"{''.join([ANSI_ESCAPE_SEQUENCES.get(i, f'(color:{i})') for i in color])}{' '.join(msg)}{ANSI_ESCAPE_SEQUENCE_END}")
    else:
        print(*msg)

THIS_SCRIPT_DIR = os.path.dirname(__file__)

ROUNDS = None
DEFAULT_OUTPUT_DIR = os.path.join(THIS_SCRIPT_DIR, 'output')
OUTPUT_DIR = None
RELATIVE_PATHS = False
arguments = []
argc = len(sys.argv)
arg_i = 1
while arg_i < argc:
    arg = sys.argv[arg_i]
    if arg[0] == '-':
        if arg == '--rounds' or arg == '-r':
            if arg_i + 1 >= argc:
                log(f"Error: Flag `--rounds / -r` is missing its value", color=['red', 'bold'])
                sys.exit(1)
            val = sys.argv[arg_i + 1]
            if not val.isdigit():
                log(f"Error: Flag `--rounds / -r` requires integer value, got non-integer value", color=['red', 'bold'])
                sys.exit(1)
            ROUNDS = val
            arg_i += 1
        elif arg == '--help' or arg == '-h':
            log(
f"""Usage: [python executable] run_turnament.py [flags] interpreter_executable programs'_direcotry
Arguments:
- interpreter_executable: Interpreter executable. Usually ./interpreter or .\\interpreter.exe (or ../interpreter, ..\\interpreter.exe when using relative paths)
                          Note: this file will be executed with `[directory]/[FILE]`, where directory is this scirpt's directory if --relative flag is present, else current working directory
- programs'_directory: Path to directory containing competing programs. All files within this directory will be considered programs regardless of file extension

Flags:
--help: Show this message and exit

--rounds / -r [rounds]: Specify the number of rounds per game (default: use default of the interpreter)

--relative / -l: Resolves all provided paths relative to the directory containing this script. (This only changes current workink directory, so full paths should still work)
--output / -o: Directory where output files will be written (default: SCIRPT_DIR/output)

--no-colors: (default on Windows) Disable colors (ANSI escape sequences)
--colors: (default on non-windows platforms) Enable colors (ANSI escape sequences)

Example usage:
python3 run_turnament.py -r 10 ../interpreter programs
py run_turnament.py --rounds 50 C:\\\\Users\\\\Barbie\\\\Documents\\\\programming\\\\turnament\\\\interpreter.exe C:\\\\Users\\\\Barbie\\\\Documents\\\\programming\\\\turnament\\\\bot_programs
""")
            sys.exit(0)
        elif arg == '--relative' or arg == '-l':
            RELATIVE_PATHS = True
        elif arg == '--no-colors':
            ENABLE_COLORS = False
        elif arg == '--colors':
            ENABLE_COLORS = True
        elif arg == '--output' or arg == '-o':
            if arg_i + 1 >= argc:
                log(f"Error: Flag `--output / -o` is missing its value", color=['red', 'bold'])
                sys.exit(1)
            OUTPUT_DIR = sys.argv[arg_i + 1]
            arg_i += 1
        else:
            log(f"Error: Unknown flag: `{arg}`", color=['red', 'bold'])
            sys.exit(1)

    else:
        arguments.append(arg)

    arg_i += 1

if len(arguments) != 2:
    log(f'Expected 2 arguments (interpreter executable command, programs\' directory), got {len(arguments)}', color=['red', 'bold'])
    sys.exit(1)

if RELATIVE_PATHS:
    os.chdir(THIS_SCRIPT_DIR)
if not OUTPUT_DIR:
    OUTPUT_DIR = DEFAULT_OUTPUT_DIR

INTERPRETER_EXECUTABLE = arguments[0]
INPUT_DIR = arguments[1]

# if os.path.exists(INTERPRETER_EXECUTABLE) and not '/' in INTERPRETER_EXECUTABLE:
#     log(f"Warning: You provided filename as executable command, if this doesn't work, consider using `./{INTERPRETER_EXECUTABLE}`\n", color=['yellow', 'bold'])
#     # sys.exit(f"Error: Interpreter executable file `{INTERPRETER_EXECUTABLE}` does not exist.")
if not os.path.exists(INTERPRETER_EXECUTABLE):
    log(f"Error: Interpreter executable file not found: {INTERPRETER_EXECUTABLE}")
    sys.exit(1)

if (not os.path.exists(INPUT_DIR)) or not os.path.isdir(INPUT_DIR):
    sys.exit(f"Error: Programs' directory `{INPUT_DIR}` does not exist or is not a directory")

#TODO add option to use non-empty directory and add option to interpreter to overwrite files
if os.path.exists(OUTPUT_DIR) and os.listdir(OUTPUT_DIR):
    log(f"Error: Specifies output directory (`{OUTPUT_DIR}`) is not empty" , color=['red', 'bold'])
    sys.exit(1)

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR) # ensure directory exists

program_paths = [file for file in os.listdir(INPUT_DIR) if not os.path.isdir(os.path.join(INPUT_DIR, file))]
processes = dict()

def remove_file_extension(filename):
    if not '.' in filename:
        return filename
    file_extension_start = len(filename) - filename[::-1].find('.')
    return filename[:file_extension_start - 1]
# Map program paths to names, TODO: add different way to specify names
PROGRAM_NAMES: dict[str, str] = {i: remove_file_extension(i) for i in program_paths}
# ensure no duplicates are present after removing file extension (e.g. program.txt, program.abc will produce duplicate names)
for key, name in [(key, name) for key, name in PROGRAM_NAMES.items()]:
    if list(PROGRAM_NAMES.values()).count(name) > 0:
        PROGRAM_NAMES[key] = key

PROGRAM_SCORES: dict[str, int] = {i:0 for i in PROGRAM_NAMES.values()}
for program1_i in range(len(program_paths)):
    program1_path = program_paths[program1_i]
    program1_name = PROGRAM_NAMES[program1_path]
    for program2_i in range(program1_i + 1, len(program_paths)):
        program2_path = program_paths[program2_i]
        program2_name = PROGRAM_NAMES[program2_path]
        output_file = os.path.join(OUTPUT_DIR, f"match_{program1_name}_vs_{program2_name}.txt")
        interpreter_dir = os.getcwd()
        process_command = [f"{interpreter_dir}/{INTERPRETER_EXECUTABLE}", program1_path, program2_path, '-o', output_file, '-n', program1_name, program2_name, '--no-log']
        if ROUNDS:
            process_command += ['-r', ROUNDS]
        
        log(f"Running command: `{' '.join(process_command)}` in {INPUT_DIR}")
        process = subprocess.Popen(process_command, stdout=subprocess.PIPE, text=True, cwd=INPUT_DIR)
        
        processes[(program1_name, program2_name)] = process

log()
for programs, process in processes.items():
    log(f"Waiting for process {process.pid} to finish (`{programs[0]}` vs `{programs[1]}`)...")
    process.wait()
log("All processes have finished!\n", color=['green', 'bold'])

for programs, process in processes.items():
    if process.returncode != 0:
        log(f"Process {process.pid} (programs: {', '.join(programs)}) returned non-zero code of {process.returncode}, printing stdout: ", color=['red', 'bold'])
        log(f"> {'\n> '.join(process.communicate()[0].split('\n'))}\n", color='italic')
    else:
        process_stdout = process.communicate()[0]
        try:
            match_result = list(map(int, process_stdout.split()))
        except Exception as e:
            print(f"Error occured when trying to format match result from stdout (`{process_stdout}`): {e}")
            continue
        log(f"Process {process.pid} success; Scores: {programs[0]}: {match_result[0]}, {programs[1]}: {match_result[1]}", color=['green', 'bold'])
        if match_result[0] < match_result[1]:
            PROGRAM_SCORES[programs[1]] += 1
        elif match_result[0] > match_result[1]:
            PROGRAM_SCORES[programs[0]] += 1
        else:
            # Draw
            pass

log("Done.", color=['green', 'bold'])
log()
log(f"Program scores:", color=['bold', 'blue'])
log(f"{'\n'.join(f'{name}: {score}' for name, score in PROGRAM_SCORES.items())}")

#TODO: Add flag for output location
import json
with open('scores.json', 'w') as f:
    json.dump(PROGRAM_SCORES, f)