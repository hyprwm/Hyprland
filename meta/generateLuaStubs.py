#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


@dataclass
class ApiNode:
    methods: set[str] = field(default_factory=set)
    children: dict[str, "ApiNode"] = field(default_factory=dict)


@dataclass
class ObjectClass:
    name: str
    methods: set[str] = field(default_factory=set)
    fields: dict[str, str] = field(default_factory=dict)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def find_matching_brace(text: str, open_brace_idx: int) -> int:
    depth = 0
    in_string = False
    string_char = ""
    escaped = False

    for i in range(open_brace_idx, len(text)):
        c = text[i]

        if in_string:
            if escaped:
                escaped = False
                continue

            if c == "\\":
                escaped = True
                continue

            if c == string_char:
                in_string = False
            continue

        if c in ('"', "'"):
            in_string = True
            string_char = c
            continue

        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i

    raise ValueError("Unbalanced braces while parsing C++ source")


def extract_function_bodies(source: str, header_pattern: re.Pattern[str]) -> list[tuple[re.Match[str], str]]:
    out: list[tuple[re.Match[str], str]] = []
    for match in header_pattern.finditer(source):
        open_idx = source.find("{", match.end() - 1)
        if open_idx < 0:
            continue

        close_idx = find_matching_brace(source, open_idx)
        out.append((match, source[open_idx + 1 : close_idx]))

    return out


def merge_node(dst: ApiNode, src: ApiNode) -> None:
    dst.methods |= src.methods
    for key, child in src.children.items():
        if key not in dst.children:
            dst.children[key] = child
        else:
            merge_node(dst.children[key], child)


def parse_binding_tree(root: Path) -> tuple[ApiNode, set[str]]:
    lua_dir = root / "src/config/lua"

    root_node = ApiNode()
    callable_namespaces: set[str] = set()

    register_header = re.compile(
        r"void\s+(?:(?:Config::Lua::Bindings::)?Internal::)?register\w+Bindings\s*\([^)]*\)\s*\{", re.MULTILINE
    )
    set_fn = re.compile(r'(?:Internal::)?set(?:Mgr)?Fn\(\s*L\s*,(?:\s*mgr\s*,)?\s*"([^"]+)"\s*,')
    set_field = re.compile(r'lua_setfield\(L,\s*-2,\s*"([^"]+)"\s*\);')

    for cpp in sorted(lua_dir.rglob("*.cpp")):
        source = read_text(cpp)
        for _, body in extract_function_bodies(source, register_header):
            local_root = ApiNode()
            stack: list[ApiNode] = [local_root]

            if re.search(
                r'lua_setfield\(L,\s*-2,\s*"__call"\s*\);.*?lua_setfield\(L,\s*-2,\s*"([^"]+)"\s*\);',
                body,
                flags=re.DOTALL,
            ):
                for ns in re.findall(
                    r'lua_setfield\(L,\s*-2,\s*"__call"\s*\);.*?lua_setfield\(L,\s*-2,\s*"([^"]+)"\s*\);',
                    body,
                    flags=re.DOTALL,
                ):
                    callable_namespaces.add(ns)

            for raw_line in body.splitlines():
                line = raw_line.strip()
                if not line:
                    continue

                if "lua_newtable(L)" in line:
                    stack.append(ApiNode())
                    continue

                m = set_fn.search(line)
                if m:
                    stack[-1].methods.add(m.group(1))
                    continue

                if "lua_setmetatable(L" in line:
                    if len(stack) > 1:
                        stack.pop()
                    continue

                m = set_field.search(line)
                if m:
                    field_name = m.group(1)
                    if field_name == "__call":
                        continue

                    if len(stack) > 1:
                        node = stack.pop()
                        if field_name in stack[-1].children:
                            merge_node(stack[-1].children[field_name], node)
                        else:
                            stack[-1].children[field_name] = node

            merge_node(root_node, local_root)

    return root_node, callable_namespaces


def parse_object_classes(root: Path) -> dict[str, ObjectClass]:
    objects_dir = root / "src/config/lua/objects"
    mt_regex = re.compile(r'static constexpr const char\* MT = "([^"]+)";')
    index_header = re.compile(r"static int\s+\w*Index\s*\(lua_State\* L\)\s*\{", re.MULTILINE)
    cond_regex = re.compile(r"(?:if|else\s+if)\s*\(([^)]*\bkey\b[^)]*)\)")
    push_class_regex = re.compile(r"Objects::CLua([A-Za-z0-9_]+)::push")

    out: dict[str, ObjectClass] = {}

    for cpp in sorted(objects_dir.glob("*.cpp")):
        source = read_text(cpp)
        mt_match = mt_regex.search(source)
        if not mt_match:
            continue

        mt_name = mt_match.group(1)
        if not mt_name.startswith("HL."):
            continue

        class_name = mt_name
        obj = ObjectClass(name=class_name)

        bodies = extract_function_bodies(source, index_header)
        if not bodies:
            out[class_name] = obj
            continue

        body = bodies[0][1]
        cond_matches = list(cond_regex.finditer(body))

        for i, cond in enumerate(cond_matches):
            start = cond.start()
            end = cond_matches[i + 1].start() if i + 1 < len(cond_matches) else len(body)
            segment = body[start:end]

            keys = re.findall(r'"([^"]+)"', cond.group(1))
            if not keys:
                continue

            is_method = "lua_pushcfunction" in segment

            if is_method:
                for key in keys:
                    obj.methods.add(key)
                continue

            inferred_types: set[str] = set()
            if "lua_pushboolean" in segment:
                inferred_types.add("boolean")
            if "lua_pushstring" in segment or "lua_pushfstring" in segment:
                inferred_types.add("string")
            if "lua_pushinteger" in segment:
                inferred_types.add("integer")
            if "lua_pushnumber" in segment:
                inferred_types.add("number")
            if "lua_newtable" in segment:
                inferred_types.add("table")
            if "lua_pushnil" in segment:
                inferred_types.add("nil")

            for pushed in push_class_regex.findall(segment):
                inferred_types.add(f"HL.{pushed}")

            if not inferred_types:
                type_str = "any"
            else:
                ordered = sorted(inferred_types, key=lambda t: (t == "nil", t))
                type_str = "|".join(ordered)

            for key in keys:
                if key in obj.fields:
                    existing = set(obj.fields[key].split("|"))
                    merged = existing | set(type_str.split("|"))
                    obj.fields[key] = "|".join(sorted(merged, key=lambda t: (t == "nil", t)))
                else:
                    obj.fields[key] = type_str

        out[class_name] = obj

    return out


def lua_type_from_config_ctor(ctor: str) -> str:
    mapping = {
        "CLuaConfigBool": "boolean",
        "CLuaConfigInt": "integer|boolean",
        "CLuaConfigFloat": "number|boolean",
        "CLuaConfigString": "string",
        "CLuaConfigColor": "string",
        "CLuaConfigVec2": "HL.Vec2Like",
        "CLuaConfigCssGap": "integer|HL.CssGap",
        "CLuaConfigFontWeight": "integer|string",
        "CLuaConfigGradient": "string|HL.Gradient",
    }
    return mapping.get(ctor, "any")


def parse_config_values(root: Path) -> dict[str, str]:
    cfg = root / "src/config/values/ConfigValues.cpp"
    source = read_text(cfg)
    pattern = re.compile(r'MS<([A-Za-z0-9_]+)>\("([^"]+)"')

    type_map = {
        "Bool": "boolean",
        "Int": "integer|boolean",
        "Float": "number|boolean",
        "String": "string",
        "Color": "string",
        "Vec2": "HL.Vec2Like",
        "CssGap": "integer|HL.CssGap",
        "FontWeight": "integer|string",
        "Gradient": "string|HL.Gradient",
    }

    out: dict[str, str] = {}
    for vtype, key in pattern.findall(source):
        out[key.replace(":", ".").replace("-", "_")] = type_map.get(vtype, "any")

    return out


def extract_initializer_body(source: str, array_name: str) -> str:
    marker = f"{array_name}[]"
    idx = source.find(marker)
    if idx < 0:
        return ""

    eq_idx = source.find("=", idx)
    if eq_idx < 0:
        return ""

    open_idx = source.find("{", eq_idx)
    if open_idx < 0:
        return ""

    close_idx = find_matching_brace(source, open_idx)
    return source[open_idx + 1 : close_idx]


def parse_descriptor_fields(root: Path) -> dict[str, dict[str, str]]:
    source = read_text(root / "src/config/lua/bindings/LuaBindingsConfigRules.cpp")
    arrays = {
        "MONITOR_FIELDS": "HL.MonitorSpec",
        "DEVICE_FIELDS": "HL.DeviceSpec",
        "WORKSPACE_RULE_FIELDS": "HL.WorkspaceRuleSpec",
        "WINDOW_RULE_EFFECT_DESCS": "HL.WindowRuleSpec",
        "LAYER_RULE_EFFECT_DESCS": "HL.LayerRuleSpec",
    }

    entry_regex = re.compile(
        r'\{\s*"([^"]+)"\s*,\s*\[\]\(\)\s*->\s*ILuaConfigValue\*\s*\{\s*return\s+new\s+([A-Za-z0-9_]+)\((.*?)\);\s*\}',
        re.DOTALL,
    )

    out: dict[str, dict[str, str]] = {class_name: {} for class_name in arrays.values()}

    for array_name, class_name in arrays.items():
        body = extract_initializer_body(source, array_name)
        if not body:
            continue

        for name, ctor, _ in entry_regex.findall(body):
            out[class_name][name] = lua_type_from_config_ctor(ctor)

    # required / conventional fields not included in descriptor arrays
    out["HL.MonitorSpec"]["output"] = "string"
    out["HL.DeviceSpec"]["name"] = "string"
    out["HL.WorkspaceRuleSpec"]["workspace"] = "string"
    out["HL.WorkspaceRuleSpec"]["enabled"] = "boolean"
    out["HL.WorkspaceRuleSpec"]["layout_opts"] = "table<string, string|number|boolean>"
    out["HL.WindowRuleSpec"]["name"] = "string"
    out["HL.WindowRuleSpec"]["enabled"] = "boolean"
    out["HL.WindowRuleSpec"]["match"] = "table<string, string|number|boolean>"
    out["HL.LayerRuleSpec"]["name"] = "string"
    out["HL.LayerRuleSpec"]["enabled"] = "boolean"
    out["HL.LayerRuleSpec"]["match"] = "table<string, string|boolean>"

    return out


def parse_known_events(root: Path) -> list[str]:
    source = read_text(root / "src/config/lua/LuaEventHandler.cpp")
    block_match = re.search(
        r"static const std::unordered_set<std::string> EVENTS = \{(.*?)\};",
        source,
        flags=re.DOTALL,
    )
    if not block_match:
        return []

    events = sorted(set(re.findall(r'"([^"]+)"', block_match.group(1))))
    return events


def helper_to_lua_type(helper: str) -> str:
    mapping = {
        "Str": "string",
        "Num": "number",
        "Bool": "boolean",
        "Monitor": "HL.MonitorSelector",
        "Workspace": "HL.WorkspaceSelector",
        "Window": "HL.WindowSelector",
        "MonitorSelector": "string",
        "WorkspaceSelector": "string",
        "WindowSelector": "string",
    }
    return mapping.get(helper, "any")


def query_struct_to_type(struct_name: str) -> str:
    name = struct_name
    if name.startswith("S") and len(name) > 1:
        name = name[1:]
    if name.endswith("Query"):
        name = name + "Filter"
    return f"HL.{name}"


def parse_query_filter_types(root: Path) -> tuple[dict[str, dict[str, str]], dict[str, str]]:
    source = read_text(root / "src/config/lua/bindings/LuaBindingsQuery.cpp")

    parse_header = re.compile(
        r"static void\s+(\w+)\s*\(\s*lua_State\* L,\s*int idx,\s*const char\* fnName,\s*(\w+)&\s*\w+\s*\)\s*\{"
    )

    query_types: dict[str, dict[str, str]] = {}
    parse_fn_to_type: dict[str, str] = {}

    for m, body in extract_function_bodies(source, parse_header):
        parse_fn = m.group(1)
        struct_name = m.group(2)
        type_name = query_struct_to_type(struct_name)
        parse_fn_to_type[parse_fn] = type_name
        query_types.setdefault(type_name, {})

        direct_assign = re.finditer(
            r'query\.([A-Za-z_][A-Za-z0-9_]*)\s*=\s*Internal::tableOpt([A-Za-z_]+)\(L,\s*idx,\s*"([^"]+)"',
            body,
        )
        for dm in direct_assign:
            field_name = dm.group(3)
            helper = dm.group(2)
            query_types[type_name][field_name] = helper_to_lua_type(helper)

        helper_calls = re.finditer(r'Internal::tableOpt([A-Za-z_]+)\(L,\s*idx,\s*"([^"]+)"', body)
        for hm in helper_calls:
            helper = hm.group(1)
            field_name = hm.group(2)
            query_types[type_name].setdefault(field_name, helper_to_lua_type(helper))

    api_overrides: dict[str, str] = {}
    for parse_fn, type_name in parse_fn_to_type.items():
        for api in re.findall(rf'{parse_fn}\(L,\s*1,\s*"([^"]+)"\s*,', source):
            if api == "hl.get_windows":
                api_overrides[api] = f"fun(filters?: {type_name}): HL.Window[]"
            elif api == "hl.get_layers":
                api_overrides[api] = f"fun(filters?: {type_name}): HL.LayerSurface[]"
            else:
                api_overrides[api] = f"fun(filters?: {type_name}): any"

    return query_types, api_overrides


def namespace_class_name(path: list[str]) -> str:
    if not path:
        return "HL.API"
    parts = [p[:1].upper() + p[1:] for p in path]
    return f"HL.{''.join(parts)}Namespace"


def format_union_alias(name: str, values: Iterable[str]) -> list[str]:
    values = list(values)
    if not values:
        return []

    lines = [f"---@alias {name}"]
    for value in values:
        lines.append(f'---| "{value}"')
    return lines


def emit_class_block(class_name: str, fields: list[tuple[str, str, bool]], operator_call: str | None = None) -> list[str]:
    lines = [f"---@class {class_name}"]
    if operator_call:
        lines.append(f"---@operator call:{operator_call}")

    for field_name, type_name, optional in fields:
        if field_name.startswith("[") and field_name.endswith("]"):
            # preformatted index field, e.g. [string]
            lines.append(f"---@field {field_name} {type_name}")
            continue

        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", field_name):
            suffix = "?" if optional else ""
            lines.append(f"---@field {field_name}{suffix} {type_name}")
            continue

        quoted = field_name.replace("'", "\\'")
        type_with_optional = f"{type_name}|nil" if optional else type_name
        lines.append(f"---@field ['{quoted}'] {type_with_optional}")

    local_name = "__" + class_name.replace(".", "_")
    lines.append(f"local {local_name} = {{}}")
    return lines


def generate_stub(root: Path) -> str:
    api_tree, callable_namespaces = parse_binding_tree(root)
    object_classes = parse_object_classes(root)
    config_values = parse_config_values(root)
    descriptor_classes = parse_descriptor_fields(root)
    events = parse_known_events(root)
    query_types, query_overrides = parse_query_filter_types(root)

    api_signatures: dict[str, str] = {
        "hl.on": "fun(event: HL.EventName, cb: fun(...)): HL.EventSubscription",
        "hl.bind": "fun(keys: string, dispatcher: HL.Dispatcher|function, opts?: HL.BindOptions): HL.Keybind",
        "hl.dispatch": "fun(dispatcher: HL.Dispatcher|function): any",
        "hl.define_submap": "fun(name: string, reset_or_fn: string|function, fn?: function): nil",
        "hl.timer": "fun(callback: function, opts: HL.TimerOptions): HL.Timer",
        "hl.config": "fun(config: table): nil",
        "hl.get_config": "fun(key: HL.ConfigKey|string): any, string?",
        "hl.device": "fun(spec: HL.DeviceSpec): nil",
        "hl.monitor": "fun(spec: HL.MonitorSpec): nil",
        "hl.window_rule": "fun(spec: HL.WindowRuleSpec): HL.WindowRule",
        "hl.layer_rule": "fun(spec: HL.LayerRuleSpec): HL.LayerRule",
        "hl.workspace_rule": "fun(spec: HL.WorkspaceRuleSpec): nil",
        "hl.permission": "fun(spec: HL.PermissionSpec): nil",
        "hl.gesture": "fun(spec: HL.GestureSpec): nil",
        "hl.get_windows": "fun(filters?: HL.WindowQueryFilter): HL.Window[]",
        "hl.get_window": "fun(selector: HL.WindowSelector): HL.Window|nil",
        "hl.get_active_window": "fun(): HL.Window|nil",
        "hl.get_urgent_window": "fun(): HL.Window|nil",
        "hl.get_workspaces": "fun(): HL.Workspace[]",
        "hl.get_workspace": "fun(selector: HL.WorkspaceSelector): HL.Workspace|nil",
        "hl.get_active_workspace": "fun(monitor?: HL.MonitorSelector): HL.Workspace|nil",
        "hl.get_active_special_workspace": "fun(monitor?: HL.MonitorSelector): HL.Workspace|nil",
        "hl.get_monitors": "fun(): HL.Monitor[]",
        "hl.get_monitor": "fun(selector: HL.MonitorSelector): HL.Monitor|nil",
        "hl.get_active_monitor": "fun(): HL.Monitor|nil",
        "hl.get_monitor_at": "fun(x: number|HL.Vec2, y?: number): HL.Monitor|nil",
        "hl.get_monitor_at_cursor": "fun(): HL.Monitor|nil",
        "hl.get_layers": "fun(filters?: HL.LayerQueryFilter): HL.LayerSurface[]",
        "hl.get_workspace_windows": "fun(workspace: HL.WorkspaceSelector): HL.Window[]",
        "hl.get_cursor_pos": "fun(): HL.Vec2|nil",
        "hl.get_last_window": "fun(): HL.Window|nil",
        "hl.get_last_workspace": "fun(monitor?: HL.MonitorSelector): HL.Workspace|nil",
        "hl.get_current_submap": "fun(): string",
        "hl.notification.create": "fun(opts?: HL.NotificationOptions): HL.Notification",
        "hl.notification.get": "fun(): HL.Notification[]",
        "hl.layout.register": "fun(name: string, provider: HL.LayoutProvider): nil",
        "hl.exec_cmd": "fun(cmd: string, rules?: table<string, string|number|boolean>): nil",
    }
    api_signatures.update(query_overrides)

    lines: list[str] = []
    lines.append("-- This file is autogenerated. Do not edit by hand.")
    lines.append("-- Generator: scripts/generateLuaStubs.py")
    lines.append("---@meta")
    lines.append("")

    lines.extend(format_union_alias("HL.EventName", events))
    lines.append("")
    lines.extend(format_union_alias("HL.ConfigKey", sorted(config_values.keys())))
    lines.append("")
    lines.append("---@alias HL.MonitorSelector string|integer|HL.Monitor")
    lines.append("---@alias HL.WorkspaceSelector string|integer|HL.Workspace")
    lines.append("---@alias HL.WindowSelector string|integer|HL.Window")
    lines.append("---@alias HL.Vec2Like HL.Vec2|{x:number, y:number}|{number, number}|string")
    lines.append("---@alias HL.CssGap integer|{top?:integer, right?:integer, bottom?:integer, left?:integer}")
    lines.append("---@alias HL.Gradient string|{colors:string[], angle?:number}")
    lines.append("")
    lines.append("---@class HL.Dispatcher")
    lines.append("local __HL_Dispatcher = {}")
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.Vec2",
            [
                ("x", "number", False),
                ("y", "number", False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.Box",
            [
                ("x", "number", False),
                ("y", "number", False),
                ("w", "number", False),
                ("h", "number", False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.LayoutTarget",
            [
                ("index", "integer", False),
                ("window", "HL.Window|nil", False),
                ("box", "HL.Box", False),
                ("place", "fun(self: HL.LayoutTarget, box: HL.Box): nil", False),
                ("set_box", "fun(self: HL.LayoutTarget, box: HL.Box): nil", False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.LayoutContext",
            [
                ("area", "HL.Box", False),
                ("targets", "HL.LayoutTarget[]", False),
                ("grid_cell", "fun(self: HL.LayoutContext, i: integer, cols: integer, rows?: integer): HL.Box", False),
                ("column", "fun(self: HL.LayoutContext, i: integer, n: integer): HL.Box", False),
                ("row", "fun(self: HL.LayoutContext, i: integer, n: integer): HL.Box", False),
                ("split", "fun(self: HL.LayoutContext, box: HL.Box, side: 'left'|'right'|'top'|'bottom'|'up'|'down', ratio: number): HL.Box", False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.LayoutProvider",
            [
                ("recalculate", "fun(ctx: HL.LayoutContext): nil", False),
                ("layout_msg", "fun(ctx: HL.LayoutContext, msg: string): boolean|string|nil", True),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.BindOptions",
            [
                ("repeating", "boolean", True),
                ("locked", "boolean", True),
                ("release", "boolean", True),
                ("non_consuming", "boolean", True),
                ("transparent", "boolean", True),
                ("ignore_mods", "boolean", True),
                ("dont_inhibit", "boolean", True),
                ("long_press", "boolean", True),
                ("submap_universal", "boolean", True),
                ("click", "boolean", True),
                ("drag", "boolean", True),
                ("description", "string", True),
                ("desc", "string", True),
                ("device", "{inclusive?: boolean, list?: string[]}", True),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.TimerOptions",
            [
                ("timeout", "integer", False),
                ("type", '"repeat"|"oneshot"', False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.GestureSpec",
            [
                ("fingers", "integer", False),
                ("direction", "string", False),
                ("action", "string", False),
                ("mods", "string", True),
                ("scale", "number", True),
                ("mode", "string", True),
                ("zoom_level", "number", True),
                ("workspace_name", "string", True),
                ("disable_inhibit", "boolean", True),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.PermissionSpec",
            [
                ("binary", "string", False),
                ("type", "string", False),
                ("allow", "string", False),
            ],
        )
    )
    lines.append("")

    lines.extend(
        emit_class_block(
            "HL.NotificationOptions",
            [
                ("color", "string", True),
                ("timeout", "number", True),
                ("icon", "integer|string", True),
                ("font_size", "number", True),
            ],
        )
    )
    lines.append("")

    for class_name in sorted(query_types.keys()):
        fields = [(name, typ, True) for name, typ in sorted(query_types[class_name].items())]
        lines.extend(emit_class_block(class_name, fields))
        lines.append("")

    for class_name in sorted(descriptor_classes.keys()):
        required_fields = {
            ("HL.MonitorSpec", "output"),
            ("HL.DeviceSpec", "name"),
            ("HL.WorkspaceRuleSpec", "workspace"),
        }
        fields: list[tuple[str, str, bool]] = []
        for key, typ in sorted(descriptor_classes[class_name].items()):
            optional = (class_name, key) not in required_fields
            fields.append((key, typ, optional))
        lines.extend(emit_class_block(class_name, fields))
        lines.append("")

    for class_name in sorted(object_classes.keys()):
        obj = object_classes[class_name]
        fields: list[tuple[str, str, bool]] = []
        for key in sorted(obj.methods):
            fields.append((key, f"fun(self: {class_name}, ...): any", False))
        for key, typ in sorted(obj.fields.items()):
            if key in obj.methods:
                continue
            fields.append((key, typ, False))

        lines.extend(emit_class_block(class_name, fields))
        lines.append("")

    def emit_namespace(node: ApiNode, path: list[str]) -> None:
        class_name = namespace_class_name(path)
        fields: list[tuple[str, str, bool]] = []

        full_prefix = "hl" + ("." + ".".join(path) if path else "")

        for method in sorted(node.methods):
            full_name = f"{full_prefix}.{method}"
            default_method_type = "fun(...): HL.Dispatcher" if path and path[0] == "dsp" else "fun(...): any"
            method_type = api_signatures.get(full_name, default_method_type)
            fields.append((method, method_type, False))

        for child_name in sorted(node.children.keys()):
            fields.append((child_name, namespace_class_name(path + [child_name]), False))

        if path == ["plugin"]:
            fields.append(("[string]", "any", False))

        operator_call = None
        if path and path[-1] in callable_namespaces:
            operator_call = "fun(...): any"

        lines.extend(emit_class_block(class_name, fields, operator_call=operator_call))
        lines.append("")

        for child_name in sorted(node.children.keys()):
            emit_namespace(node.children[child_name], path + [child_name])

    emit_namespace(api_tree, [])

    lines.append("---@type HL.API")
    lines.append("hl = {}")
    lines.append("")

    # include a tiny map of config key value types for users who query values dynamically
    lines.append("---@class HL.ConfigValueTypes")
    for key, typ in sorted(config_values.items()):
        lines.append(f"---@field ['{key}'] {typ}")
    lines.append("local __HL_ConfigValueTypes = {}")
    lines.append("")

    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists():
        existing = read_text(path)
        if existing == content:
            return False

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate LuaLS stubs for Hyprland Lua config API")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Output .lua stub path (defaults to ./meta/hl.meta.lua)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check mode: fail if output differs from generated content",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve() if args.output else root / "meta/hl.meta.lua"

    content = generate_stub(root)

    if args.check:
        if not output.exists():
            print(f"[lua-stubs] missing generated file: {output}", file=sys.stderr)
            return 1

        existing = read_text(output)
        if existing != content:
            print(f"[lua-stubs] generated stubs are out of date: {output}", file=sys.stderr)
            return 1

        print(f"[lua-stubs] up to date: {output}")
        return 0

    changed = write_if_changed(output, content)
    state = "updated" if changed else "unchanged"
    print(f"[lua-stubs] {state}: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
