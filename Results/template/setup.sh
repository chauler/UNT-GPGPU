#! /bin/csh -f

setenv filepath `pwd`
setenv config_path `echo "$filepath" | sed 's_'$results/'__'`

sh setup_ALL.sh $config_path;
