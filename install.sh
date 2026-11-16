#!/data/data/com.termux/files/usr/bin/env bash
# Build and install Plusafetch with backups, completions, and a recorded
# uninstall path. Termux installs into $PREFIX; other systems into ~/.local.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -n "${PREFIX:-}" ]; then PREFIX_DIR="$PREFIX"; else PREFIX_DIR="$HOME/.local"; fi
BUILD_DIR="$REPO_DIR/build"
JOBS="${JOBS:-2}"
DRY=0
NO_DEPS=0
NO_COMPLETIONS=0
UNINSTALL=0
FORCE=0
YES=0
MENU=1
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/plusafetch"
MANIFEST="$STATE_DIR/installed.tsv"

usage() {
    cat <<EOF
Usage: ./install.sh [options]

Install:
  --prefix DIR       install prefix (default: \$PREFIX, else ~/.local)
  --no-deps          never install missing compiler/cmake packages
  --no-completions   skip bash/zsh completions
  --dry-run          print what would be built and installed
  --yes              skip menus and the uninstall confirmation
  --no-menu          never show the interactive fzf menu

Uninstall:
  --uninstall        remove files recorded by this installer
  --force            remove recorded files even if edited since install

  -h, --help         show this help
EOF
}

info() { printf '\033[38;2;120;200;255m[plusafetch]\033[0m %s\n' "$*"; }
warn() { printf '\033[38;2;230;150;60m[plusafetch]\033[0m %s\n' "$*" >&2; }
die()  { warn "$*"; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix) shift; [ "$#" -gt 0 ] || die "--prefix needs a directory"; PREFIX_DIR="$1" ;;
        --prefix=*) PREFIX_DIR="${1#*=}" ;;
        --no-deps) NO_DEPS=1 ;;
        --no-completions) NO_COMPLETIONS=1 ;;
        --dry-run) DRY=1 ;;
        --uninstall) UNINSTALL=1 ;;
        --force) FORCE=1 ;;
        --yes|-y) YES=1; MENU=0 ;;
        --no-menu) MENU=0 ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

case "$PREFIX_DIR" in
    /*) ;;
    *) PREFIX_DIR="$PWD/$PREFIX_DIR" ;;
esac
BIN_DIR="$PREFIX_DIR/bin"
TARGET="$BIN_DIR/plusafetch"
SHARE="$PREFIX_DIR/share"

hash_file() {
    if have sha256sum; then sha256sum "$1" | cut -d' ' -f1
    elif have shasum; then shasum -a 256 "$1" | cut -d' ' -f1
    elif have cksum; then cksum "$1" | awk '{print $1 "-" $2}'
    else printf 'unknown'
    fi
}

manifest_upsert() {
    local path="$1" tmp
    mkdir -p "$STATE_DIR"
    tmp="$MANIFEST.$$"
    { [ -f "$MANIFEST" ] && awk -F'\t' -v p="$path" '$1 != p' "$MANIFEST" || true; } > "$tmp"
    printf '%s\t%s\n' "$path" "$(hash_file "$path")" >> "$tmp"
    mv -f "$tmp" "$MANIFEST"
}

install_file() { # <source> <destination>
    local src="$1" dst="$2" bak
    if [ "$DRY" = "1" ]; then
        printf '  would install %s -> %s\n' "${src#"$REPO_DIR"/}" "$dst"
        return 0
    fi
    mkdir -p "$(dirname "$dst")"
    if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
        printf '  unchanged: %s\n' "$dst"
        manifest_upsert "$dst"
        return 0
    fi
    if [ -e "$dst" ]; then
        bak="$dst.bak-$(date +%Y%m%d-%H%M%S)-$$"
        cp -p "$dst" "$bak"
        printf '  backup: %s\n' "$bak"
    fi
    cp -p "$src" "$dst"
    manifest_upsert "$dst"
    printf '  installed: %s\n' "$dst"
}

if [ "$UNINSTALL" = "1" ]; then
    [ -f "$MANIFEST" ] || { info "no files recorded by this installer"; exit 0; }
    if [ "$YES" = "0" ] && [ "$DRY" = "0" ]; then
        if [ -t 0 ]; then
            printf 'Remove files recorded in %s? [y/N] ' "$MANIFEST"
            read -r reply
            case "$reply" in [Yy]*) ;; *) echo "Canceled."; exit 0 ;; esac
        else
            die "use --yes with --uninstall in a non-interactive shell"
        fi
    fi
    removed=0; kept=0; tmp="$MANIFEST.$$"; tmp_old="$tmp.old"
    cat "$MANIFEST" > "$tmp_old"
    # Remove in reverse install order: a recorded symlink (the plusfet alias)
    # hashes by following to its target, which may itself be recorded earlier
    # in the manifest. Deleting the target first would break that verification.
    mapfile -t mlines < "$tmp_old"
    for (( i=${#mlines[@]} - 1; i >= 0; i-- )); do
        IFS=$'\t' read -r path old_hash <<< "${mlines[i]}"
        [ -n "$path" ] || continue
        [ -e "$path" ] || [ -L "$path" ] || continue
        if [ "$FORCE" = "0" ] && [ "$(hash_file "$path")" != "$old_hash" ]; then
            warn "keeping modified file: $path (use --force to remove)"
            printf '%s\t%s\n' "$path" "$old_hash" >> "$tmp"
            kept=$((kept + 1)); continue
        fi
        if [ "$DRY" = "1" ]; then
            printf '  would remove: %s\n' "$path"
        else
            rm -f "$path"
            printf '  removed: %s\n' "$path"
        fi
        removed=$((removed + 1))
    done
    if [ "$DRY" = "0" ]; then
        if [ "$kept" -eq 0 ]; then rm -f "$MANIFEST"; rmdir "$STATE_DIR" 2>/dev/null || true
        else mv -f "$tmp" "$MANIFEST"; fi
    fi
    rm -f "$tmp" "$tmp_old"
    info "uninstall complete: $removed removed, $kept kept"
    exit 0
fi

if [ "$DRY" = "1" ]; then
    printf '  would build with cmake in %s (jobs: %s)\n' "$BUILD_DIR" "$JOBS"
    install_file "$BUILD_DIR/plusafetch" "$TARGET"
    [ "$NO_COMPLETIONS" = "1" ] || {
        install_file "$REPO_DIR/completions/plusafetch.bash" "$SHARE/bash-completion/completions/plusafetch"
        install_file "$REPO_DIR/completions/_plusafetch" "$SHARE/zsh/site-functions/_plusafetch"
        install_file "$REPO_DIR/completions/_plusafetch" "$SHARE/zsh/site-functions/_plusfet"
    }
    printf '  would link %s -> %s\n' "$BIN_DIR/plusfet" "$TARGET"
    exit 0
fi

COMP_BASH=1
COMP_ZSH=1
if [ "$NO_COMPLETIONS" = "1" ]; then COMP_BASH=0; COMP_ZSH=0; fi
if [ "$MENU" = "1" ] && [ "$NO_COMPLETIONS" = "0" ] && [ -t 0 ] && [ -t 1 ] && have fzf; then
    choice=$(printf 'Binary + both completions\nBinary only\nBinary + bash completion\nBinary + zsh completion\nCancel\n' \
        | fzf --reverse --height=40% --border --prompt='Plusafetch install> ') || choice="Cancel"
    case "$choice" in
        "Binary + both completions") ;;
        "Binary only") COMP_BASH=0; COMP_ZSH=0 ;;
        "Binary + bash completion") COMP_ZSH=0 ;;
        "Binary + zsh completion") COMP_BASH=0 ;;
        *) echo "Canceled."; exit 0 ;;
    esac
fi

NEED=()
have cmake || NEED+=(cmake)
have clang++ || NEED+=(clang)
if [ "${#NEED[@]}" -gt 0 ]; then
    if [ "$NO_DEPS" = "1" ]; then
        die "missing build dependencies: ${NEED[*]}"
    fi
    info "installing build dependencies: ${NEED[*]}"
    if [ -n "${TERMUX_VERSION:-}" ] || [ "${PREFIX:-}" = "/data/data/com.termux/files/usr" ]; then
        pkg install -y "${NEED[@]}" || die "pkg install failed"
    elif have apt-get; then
        sudo apt-get install -y "${NEED[@]}" || die "apt install failed"
    elif have dnf; then
        sudo dnf install -y "${NEED[@]}" || die "dnf install failed"
    elif have pacman; then
        sudo pacman -S --noconfirm "${NEED[@]}" || die "pacman install failed"
    else
        die "install ${NEED[*]} manually, or use --no-deps"
    fi
fi
have cmake || die "cmake is still unavailable"
have clang++ || die "clang++ is still unavailable"

info "building with cmake (jobs: $JOBS)"
cmake -S "$REPO_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j "$JOBS"
[ -x "$BUILD_DIR/plusafetch" ] || die "build did not produce $BUILD_DIR/plusafetch"

install_file "$BUILD_DIR/plusafetch" "$TARGET"
if [ "$COMP_BASH" = "1" ]; then install_file "$REPO_DIR/completions/plusafetch.bash" "$SHARE/bash-completion/completions/plusafetch"; fi
if [ "$COMP_ZSH" = "1" ]; then
    install_file "$REPO_DIR/completions/_plusafetch" "$SHARE/zsh/site-functions/_plusafetch"
    install_file "$REPO_DIR/completions/_plusafetch" "$SHARE/zsh/site-functions/_plusfet"
fi
ALIAS="$BIN_DIR/plusfet"
if [ -e "$ALIAS" ] && [ ! -L "$ALIAS" ]; then
    bak="$ALIAS.bak-$(date +%Y%m%d-%H%M%S)-$$"
    cp -p "$ALIAS" "$bak"
    printf '  backup: %s\n' "$bak"
fi
ln -sfn plusafetch "$ALIAS"
manifest_upsert "$ALIAS"
printf '  linked: %s -> plusafetch\n' "$ALIAS"

if "$TARGET" --version >/dev/null 2>&1; then
    info "installed: $("$TARGET" --version 2>/dev/null | head -n1)"
else
    warn "installed binary failed --version; check $TARGET"
fi
case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) printf 'Add %s to PATH to run plusafetch.\n' "$BIN_DIR" ;;
esac
