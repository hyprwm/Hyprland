#!/bin/sh

generate_grammar() {
	cat hyprctl.usage
	echo
	printf '<CONFIG_OPTION> ::= '
	sed -n 's/.*configValues\["\(.*\)"\].*/    | \1/p' ../src/config/ConfigManager.cpp |
		sort | uniq | sed 's/\./\\./g' | tail -c +7
	echo ';'
}

generate_grammar > generated/hyprctl.usage

cd generated || exit 1
complgen compile --bash-script hyprctl.bash hyprctl.usage
complgen compile --zsh-script  hyprctl.zsh  hyprctl.usage
complgen compile --fish-script hyprctl.fish hyprctl.usage
