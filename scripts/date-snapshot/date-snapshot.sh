#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1
# Copyright (C) 2022, VMware Inc, June Knauth <june.knauth@gmail.com>
#
# Script to download one or several git repos at the last commit before a specified date
# Takes inputs specified via command line or in a file

package=date-snapshot
clean=false # clean and download define the script's behavior
download=true
# If clean is set, we delete existing repos. If not, we leave them untouched (no checkout).
# If download is set, we download and checkout the repos specified if they don't exist.

download_checkout(){
  IN="${1}"
  IFS=';' read -ra ADDR <<< "$IN"

  if $clean ; then # delete the repos if requested
    echo "Removing ${ADDR[0]}"
    rm -rf ./${ADDR[0]}
  fi

  # if download is true and directory doesn't exist
  if [[ $download = true && ! -d ${ADDR[0]} ]] ; then
    git clone -b ${ADDR[2]} ${ADDR[1]} ${ADDR[0]}
    cd ${ADDR[0]}

    last_date=`date -d ${ADDR[3]} +%s` # convert given date to unix timestamp
    # we could simply give the param to git but it understands fewer formats than date

    # this checks out the first entry in the list of commit hashes which occur before our date
    git checkout `git log --date=unix --before=${last_date} --pretty=format:"%H" | head -n 1`

    cd ..
  elif [[ $download = false ]]; then
    echo "Download flag not set, skipping ${ADDR[0]}"
  elif [[ -d ${ADDR[0]} ]] ; then
    echo "Directory exists, skipping ${ADDR[0]}"
  fi

  unset IFS
}

while test $# -gt 0; do
  case "$1" in
    -h|--help)
      echo "$package - Download a git repo and checkout the last commit before a given date"
      echo "Takes CLI and file arguments with format '<repo name>;<repo url>;<branch>;<date>'"
      echo "Dates can be provided in any format the unix 'date' command understands."
      echo " "
      echo "$package [options] [arguments]"
      echo " "
      echo "options:"
      echo "-h, --help                show brief help"
      echo "-c, --clean               delete the directories specified in the input and continue"
      echo "-d, --delete              delete the directories specified in the input and exit"
      echo "-i, --input REPOS         specify a space-sep'd list of repos to download"
      echo "-f, --file FILE           specify an input file, one repo per line"
      exit 0
      ;;
    -c|--clean)
      shift
      clean=true
      download=true
      ;;
    -d|--delete)
      shift
      clean=true
      download=false
      ;;
    -i|--input)
      shift
      IFS=" "
      while read repo; do
        download_checkout $repo
      done <<< $1
      unset IFS
      break
      ;;
    -f|--file)
      shift
      while read line; do
        download_checkout $line
      done <$1
      break
      ;;
    *)
      echo "Usage: ${package} [-c|--clean] [-d|delete] [-i|--input] [-f|--file] <input>"
      echo "${package} --help for more"
      break
      ;;
  esac
done

