
DIRECTORY=$1
KEY=$2

if [ -z "$1" ] || [ -z "$2" ]; then
     echo "Error: Parameters are empty."
     exit 1
fi


if [ ! -d "$DIRECTORY" ]; then
     echo "Error: Directory '$dir' does not exist."
     exit 1
fi

