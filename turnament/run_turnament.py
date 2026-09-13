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

PROGRAM_DIR = os.path.dirname(__file__)

ROUNDS = None
DEFUALT_OUTPUT_DIR = os.path.join(PROGRAM_DIR, 'output')
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
f"""Usage: [python executable] run_turnament.py [flags] interpreter_command programs'_direcotry
Arguments:
- interpreter_command: Command to execute the interpreter with. Usually ./interpreter or .\\interpreter.exe (or ../interpreter, ..\\interpreter.exe when using relative paths)
- programs'_directory: Path to directory containing competing programs. All files within this directory will be considered programs regardless of file extension

Flags:
--help: Show this message and exit

--rounds / -r [rounds]: Specify the number of rounds per game (default: use default of the interpreter)

--relative / -l: Resolves all provided paths relative to the directory containing this script. (This only changes current workink directory, so full paths should still work)
--output / -o: Directory where output files will be written

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
    os.chdir(PROGRAM_DIR)
if not OUTPUT_DIR:
    OUTPUT_DIR = DEFUALT_OUTPUT_DIR


if os.path.exists(arguments[0]) and not '/' in arguments[0]:
    log(f"Warning: You provided filename as executable command, if this doesn't work, consider using `./{arguments[0]}`\n", color=['yellow', 'bold'])
    # sys.exit(f"Error: Interpreter executable file `{arguments[0]}` does not exist.")

if (not os.path.exists(arguments[1])) or not os.path.isdir(arguments[1]):
    sys.exit(f"Error: Programs' directory `{arguments[1]}` does not exist or is not a directory")

#TODO add option to use non-empty directory and add option to interpreter to overwrite files
if os.path.exists(OUTPUT_DIR) and os.listdir(OUTPUT_DIR):
    log(f"Error: Specifies output directory (`{OUTPUT_DIR}`) is not empty" , color=['red', 'bold'])
    sys.exit(1)
program_paths = os.listdir(arguments[1])
processes = []

def remove_file_extension(filename):
    if not '.' in filename:
        return filename
    file_extension_start = len(filename) - filename[::-1].find('.')
    return filename[:file_extension_start - 1]
# Map program paths to names, TODO: add different way to specify names
PROGRAM_NAMES: dict[str, str] = {i: remove_file_extension(i) for i in program_paths}
for program1_i in range(len(program_paths)):
    program1 = program_paths[program1_i]
    for program2_i in range(program1_i + 1, len(program_paths)):
        program2 = program_paths[program2_i]
        output_file = os.path.join(OUTPUT_DIR, f"match_{PROGRAM_NAMES[program1]}_vs_{PROGRAM_NAMES[program2]}.txt")
        process_command = [arguments[0], program1, program2, '-o', output_file]
        if ROUNDS:
            process_command += ['-r', ROUNDS]
        
        log(f"Running command: `{' '.join(process_command)}`")
        process = subprocess.Popen(process_command, stdout=subprocess.PIPE, text=True)
        processes.append(process)

log()
for process in processes:
    log(f"Waiting for process {process.pid} to finish...")
    process.wait()
log("All processes have finished!\n", color=['green', 'bold'])

for process in processes:
    if process.returncode != 0:
        log(f"Process {process.pid} returned non-zero code of {process.returncode}, printing stdout: ", color=['red', 'bold'])
        log(f"{'\n> '.join(process.communicate()[0].split('\n'))}\n", color='italic')

log("Done.")

