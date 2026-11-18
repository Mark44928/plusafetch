# bash completion for plusafetch
_plusafetch() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        --format)   COMPREPLY=($(compgen -W "{os} {cpu} {memory} {kernel} {disk} {uptime} {shell} {terminal}" -- "$cur")); return ;;
        --modules)  COMPREPLY=($(compgen -W "title os kernel host cpu gpu memory swap loadavg disk uptime packages shell terminal datetime locale separator colors command battery brightness sensors telephony wifi clipboard" -- "$cur")); return ;;
        --theme)    COMPREPLY=($(compgen -W "greenit bluney catppuccin-mocha dracula gruvbox nord rose-pine solarized tokyo-night mono" -- "$cur")); return ;;
        --alert)    COMPREPLY=($(compgen -W "mem>80 mem>90 swap>80 disk>90 load>4 load>8 battery<20 battery<10" -- "$cur")); return ;;
        --report-file) COMPREPLY=($(compgen -f -- "$cur")); return ;;
        --html-report) COMPREPLY=($(compgen -f -- "$cur")); return ;;
        --alert-cooldown) COMPREPLY=($(compgen -W "30 60 120 300 600" -- "$cur")); return ;;
    esac

    local opts="--all --json --watch -w --format --modules --list-modules --list-themes --theme --values-only
        --verify-config --doctor --statusline --report --html-report --report-file --gen-config --notify --no-color --color
        --alert --alert-hook --alert-cooldown --check-alerts
        --version --help"
    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
}
complete -F _plusafetch plusafetch plusfet
