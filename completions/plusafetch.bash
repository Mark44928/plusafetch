# bash completion for plusafetch
_plusafetch() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        --format)   COMPREPLY=($(compgen -W "{os} {cpu} {memory} {kernel} {disk} {uptime} {shell} {terminal}" -- "$cur")); return ;;
        --modules)  COMPREPLY=($(compgen -W "title os kernel host cpu gpu memory disk uptime packages shell terminal datetime locale separator colors command battery brightness sensors telephony wifi clipboard" -- "$cur")); return ;;
    esac

    local opts="--all --json --format --modules --list-modules --values-only
        --verify-config --gen-config --notify --no-color --color
        --version --help"
    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
}
complete -F _plusafetch plusafetch
