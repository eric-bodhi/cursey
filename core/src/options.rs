/// Editor settings, changed with `:set`. To add an option: add a field here
/// and an entry in the `OPTION_DEFS` table below.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Options {
    pub tabstop: usize,
    pub shiftwidth: usize,
    pub expandtab: bool,
    pub autoindent: bool,
    pub number: bool,
    pub scrolloff: usize,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            tabstop: 4,
            shiftwidth: 4,
            expandtab: true,
            autoindent: true,
            number: true,
            scrolloff: 5,
        }
    }
}

enum Field {
    Bool(fn(&mut Options) -> &mut bool),
    /// `positive`: zero is rejected (E487).
    Num {
        get: fn(&mut Options) -> &mut usize,
        positive: bool,
    },
}

struct OptionDef {
    name: &'static str,
    abbrev: &'static str,
    field: Field,
}

static OPTION_DEFS: [OptionDef; 6] = [
    OptionDef {
        name: "tabstop",
        abbrev: "ts",
        field: Field::Num {
            get: |o| &mut o.tabstop,
            positive: true,
        },
    },
    OptionDef {
        name: "shiftwidth",
        abbrev: "sw",
        field: Field::Num {
            get: |o| &mut o.shiftwidth,
            positive: true,
        },
    },
    OptionDef {
        name: "expandtab",
        abbrev: "et",
        field: Field::Bool(|o| &mut o.expandtab),
    },
    OptionDef {
        name: "autoindent",
        abbrev: "ai",
        field: Field::Bool(|o| &mut o.autoindent),
    },
    OptionDef {
        name: "number",
        abbrev: "nu",
        field: Field::Bool(|o| &mut o.number),
    },
    OptionDef {
        name: "scrolloff",
        abbrev: "so",
        field: Field::Num {
            get: |o| &mut o.scrolloff,
            positive: false,
        },
    },
];

fn find_option(name: &str) -> Option<&'static OptionDef> {
    OPTION_DEFS
        .iter()
        .find(|def| def.name == name || def.abbrev == name)
}

impl Options {
    /// Apply one `:set` argument ("ts=8", "noet", "number", "invnu", "nu?").
    /// Returns `Ok(Some(text))` with text to display for queries like "ts?",
    /// or an error message on failure.
    pub fn set(&mut self, arg: &str) -> Result<Option<String>, String> {
        let unknown = || format!("E518: Unknown option: {arg}");

        if let Some((name, value)) = arg.split_once('=') {
            let def = find_option(name).ok_or_else(unknown)?;
            let number_required =
                || format!("E521: Number required after =: {arg}");
            let Field::Num { get, positive } = def.field else {
                return Err(number_required());
            };
            // Digits only: str::parse would also accept a leading '+'.
            if value.is_empty() || !value.bytes().all(|b| b.is_ascii_digit()) {
                return Err(number_required());
            }
            let n: usize = value.parse().map_err(|_| number_required())?;
            if n == 0 && positive {
                return Err(format!("E487: Argument must be positive: {arg}"));
            }
            *get(self) = n;
            return Ok(None);
        }

        let (arg, query) = match arg.strip_suffix('?') {
            Some(name) => (name, true),
            None => (arg, false),
        };
        enum Action {
            On,
            Off,
            Toggle,
        }
        let (def, action) = if let Some(def) = find_option(arg) {
            (def, Action::On)
        } else if let Some(name) = arg.strip_prefix("no") {
            (find_option(name).ok_or_else(unknown)?, Action::Off)
        } else if let Some(name) = arg.strip_prefix("inv") {
            (find_option(name).ok_or_else(unknown)?, Action::Toggle)
        } else {
            return Err(unknown());
        };

        let flag = match def.field {
            Field::Num { get, .. } => {
                // `:set ts` on a number option shows its value, like vim.
                return Ok(Some(format!("{}={}", def.name, get(self))));
            }
            Field::Bool(get) => get(self),
        };
        if query {
            let prefix = if *flag { "" } else { "no" };
            return Ok(Some(format!("{prefix}{}", def.name)));
        }
        *flag = match action {
            Action::On => true,
            Action::Off => false,
            Action::Toggle => !*flag,
        };
        Ok(None)
    }
}
