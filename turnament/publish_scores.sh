set -e
set -x

# Get direcotry of the shell scirpt
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cd $SCRIPT_DIR
cd output


ufw allow 80
python -m http.server 80
ufw deny 80