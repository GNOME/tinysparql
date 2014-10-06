#-*- mode: shell-script;-*-

tracker_cmds()
{
    possible=`tracker | egrep "   [a-z].*   " | awk '{ print ($1) }'`
    local cur=${COMP_WORDS[COMP_CWORD]}
    COMPREPLY=( $(compgen -W "$possible" -- $cur) )
}

complete -F tracker_cmds tracker
