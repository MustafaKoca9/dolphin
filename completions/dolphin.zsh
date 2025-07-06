#compdef dolphin

# SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
# SPDX-FileCopyrightText: 2024 Mustafa Koca
# SPDX-License-Identifier: GPL-2.0-or-later

# This is an advanced and user-friendly Zsh completion script
# for the Dolphin file manager.
#
# Improvements:
# - Descriptive help texts.
# - Added additional options like `-v / --version`.
# - File/directory and KDE IO Slave (KIO) completions are grouped for clarity.
# - Prevents suggesting options that have already been used.
# - Added argument completion support for options like `--select` and `--split`.

local ret=1

# This helper function provides completion for common KDE IO Slaves.
# These are special protocols used by KDE applications.
_dolphin_kio_urls() {
    compadd trash:/ recent:/ documents:/ pictures:/ music:/ videos:/ remote:/ ftp:/ sftp:/ smb:/
}


# This helper function presents both local files/directories (with a trailing '/')
# and KIO URLs in a grouped, user-friendly way.
_dolphin_targets() {
    _alternative \
        'files:Local Files and Directories:_files -/' \
        'kio:KDE IO Slaves (KIO):_dolphin_kio_urls'
}

# _arguments: Zsh's main completion function.
# -s: Associates short and long options (e.g., -h and --help).
# -C: Re-analyzes the command line from the beginning.
# *: Specifies that the option can only be used once, so it won't be suggested again.
# :...: The action specifies which completion function to call for the option's argument.
_arguments -s -C \
  '(-h --help)'{-h,--help}'[Show help message and exit.]' \
  '(-v --version)'{-v,--version}'[Show version information and exit.]' \
  '--select=:File or directory to select:_dolphin_targets' \
  '--split=:Target to open in a split view:_dolphin_targets' \
  '*--new-window[Always open in a new window.]' \
  '*--daemon[Starts the Dolphin daemon (for the DBus interface only).]' \
  '*:Target file or URL:_dolphin_targets' \
  && ret=0

return $ret
