#!/bin/bash

LINE_COUNT=0
FILE_COUNT=0

# Function to count occurrences of a word in a file
count_word_occurrences() {
    local word=$1
    local file=$2

    if [ ! -f "$file" ]; then
        echo "Error: File does not exist."
        return 1
    fi

    # Count the occurrences using grep and wc
    count=$(grep -o -w "$word" "$file" | wc -l)
    ((LINE_COUNT+=$count))
    # echo "The word '$word' occurs $count times in the file '$file'."
}

# Function to process files and directories
process_files() {
    for item in "$1"/*; do
        if [ -d "$item" ]; then
            # If it's a directory, call the function recursively
            process_files "$item"
        elif [ -f "$item" ]; then
            # If it's a file, do something with it
	    ((FILE_COUNT++))
            # echo "Processing file: $item"
	    count_word_occurrences $KEY $item
	fi
    done
}

DIRECTORY=$1
KEY=$2

if [ -z "$1" ] || [ -z "$2" ]; then
     # echo "Error: Parameters are empty."
     exit 0
fi


if [ ! -d "$DIRECTORY" ]; then
     # echo "Error: Directory '$dir' does not exist."
     exit 0
fi


# Call the function with the starting directory
process_files $DIRECTORY
echo "The number of files are: $FILE_COUNT The number of matching lines are: $LINE_COUNT"
