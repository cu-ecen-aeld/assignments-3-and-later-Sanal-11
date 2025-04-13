
ABS_FILE=$1
KEY=$2

if [ -z "$1" ] || [ -z "$2" ]; then
     echo "Error: Parameters are empty."
     exit 1
fi

dir_path=$(dirname "$ABS_FILE")

if [ ! -d "$dir_path" ]; then
        mkdir -p "$dir_path" || {
            echo "Error: Failed to create directory '$dir_path'" >&2
            exit 1
     }
fi

touch "$ABS_FILE" || {
     echo "Error: Failed to create file '$ABS_FILE'" >&2
     exit 1
}

echo "writestr" > "$ABS_FILE"

echo "File created: $ABS_FILE"