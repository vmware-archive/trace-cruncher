#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1
# Copyright (C) 2022, VMware Inc, June Knauth <june.knauth@gmail.com>
#
# Script to download one or several git repos at the last commit before a specified date
# Takes inputs specified via command line or in a file

package=date-snapshot
clean="false" # Delete existing repos
download="true" # Download and checkout the repos specified if they don't exist
verbose="false" # Print non-critical debug info and warnings
latest="false" # Ignore the specified date and get the latest version

# Get absolute path of the folder we're working in, derived from the file path for -f
# If that command fails, abort
get_absolute_folder(){
  name=$(dirname "$1") # directory which contains our input
  dir_valid=$(if [[ -d "${name}" ]]; then echo "true"; fi) # is this directory valid?
  cd "${name}" # attempt to cd to that directory
  if [[ $dir_valid = "true" && $? = "0" ]] ; then # if the directory is valid and cd succeeds
    echo $(pwd -P)
  else
    log_critical "*** Error: Could not get path info for input file."
  fi
}

# log_critical always prints for critical information
log_critical(){
echo "$1"
}

# log_verbose only prints when the -v flag is set
log_verbose(){
if [[ $verbose = "true" ]] ; then
echo "$1"
fi
}

# the main function to download and checkout the specified repositories
download_checkout(){
  IN="${1}"
  IFS=';' read -ra ADDR <<< "$IN"

  if [[ $clean = "true" ]] ; then # delete the repos if requested
    if [[ ! -d "${workdir}/${ADDR[0]}" ]] ; then
      log_verbose "*** Warning: Directory ${workdir}/${ADDR[0]} does not exist, nothing to delete"
    else
      log_verbose "Removing ${workdir}/${ADDR[0]}"
      rm -rf "${workdir}/${ADDR[0]}"
    fi
  fi

  # if download is true and directory doesn't exist
  if [[ $download = "true" && ! -d "${workdir}/${ADDR[0]}" ]] ; then
    git clone -b "${ADDR[2]}" "${ADDR[1]}" "${workdir}/${ADDR[0]}"
    if [[ $latest = "false" ]] ; then # If latest flag is set, leave repo as-is
      cd "${workdir}/${ADDR[0]}"
      last_date=`date -d "${ADDR[3]}" +%s` # convert given date to unix timestamp
      # we could simply give the param to git but it understands fewer formats than date
      # this checks out the first entry in the list of commit hashes which occur before our date
      git checkout `git log --date=unix --before="${last_date}" --pretty=format:"%H" | head -n 1`
      cd "${workdir}"
    else
      log_verbose "Latest flag set, skipping checkout for ${ADDR[0]}"
    fi
  elif [[ $download = "false" ]] ; then
    log_verbose "Download flag not set, skipping ${ADDR[0]}"
  elif [[ -d "${workdir}/${ADDR[0]}" ]] ; then
    log_verbose "Directory exists, skipping ${ADDR[0]}"
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
      echo "-v, --verbose             print debugging information to the console"
      echo "-l, --latest              ignore specified date and checkout the latest versions"
      echo "-i, --input REPOS         specify a space-sep'd list of repos to download"
      echo "-f, --file FILE           specify an input file, one repo per line"
      exit 0
      ;;
    -c|--clean)
      shift
      clean="true"
      download="true"
      ;;
    -d|--delete)
      shift
      clean="true"
      download="false"
      ;;
    -v|--verbose)
      shift
      verbose="true"
      ;;
    -l|--latest)
      shift
      latest="true"
      ;;
    -i|--input)
      shift
      workdir="$(pwd)"
      IFS=" "
      while read repo; do
        download_checkout $repo
      done <<< $1
      unset IFS
      break
      ;;
    -f|--file)
      shift
      workdir="$(get_absolute_folder "$1")"
      while read line; do
        download_checkout $line
      done <$1
      break
      ;;
    *)
      echo "Usage: ${package} [-c] [-d] [-v] [-l] [-i|--input] [-f|--file] <input>"
      echo "${package} --help for more"
      break
      ;;
  esac
done

