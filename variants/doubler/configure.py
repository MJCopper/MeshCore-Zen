"""Keep the last profile radio definition when inheriting upstream defaults."""

Import("env")

radio_names = {"LORA_FREQ", "LORA_BW", "LORA_SF", "LORA_CR"}
seen = set()
defines = []
for define in reversed(env.get("CPPDEFINES", [])):
    name = define[0] if isinstance(define, (list, tuple)) else define
    if name in radio_names:
        if name in seen:
            continue
        seen.add(name)
    defines.append(define)
env.Replace(CPPDEFINES=list(reversed(defines)))
