#!/bin/bash

# Script to download one or several git repos at the last commit before a specified date
# Takes inputs specified via command line or in a file
# Created by June Knauth (VMware) <june.knauth@gmail.com>, 2022-06-22

package=date-snapshot

download_checkout(){
  IN="${1}"
  # $fields=(${IN//;/ }) 
  IFS=';' read -ra ADDR <<< "$IN"
  
  git clone -b ${ADDR[2]} ${ADDR[1]} ${ADDR[0]}
  cd ${ADDR[0]}

  last_date=`date -d ${ADDR[3]} +%s` # convert given date to unix timestamp
  # we could simply give the param to git but it understands fewer formats than date
 
  # this checks out the first entry in the list of commit hashes which occur before our date 
  git checkout `git log --date=unix --before=${last_date} --pretty=format:"%H" | head -n 1`
   
  cd .. 
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
      echo "-i, --input REPOS         specify a space-sep'd list of repos to download"
      echo "-f, --file FILE           specify an input file, one repo per line"
      exit 0
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
      echo "Usage: ${package} [-s] [-i|--input] [-f|--file]"
      echo "${package} --help for more"
      break
      ;;
  esac
done

