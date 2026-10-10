#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Register {
    pub text: String,
    /// Linewise registers hold whole lines ("line1\nline2", no trailing '\n')
    /// and are pasted as new lines above/below the cursor line.
    pub linewise: bool,
}

/// Vim register semantics (subset):
///   ""       unnamed: the last yank or delete
///   "0       last yank
///   "1-"9    last multi-line/linewise deletes, shifted on each delete
///   "-       last small (within one line) delete
///   "a-"z    named; "A-"Z append to the lowercase one
///   "_       black hole: writes are discarded
#[derive(Debug)]
pub struct Registers {
    unnamed: Register,
    small_delete: Register,
    numbered: [Register; 10],
    named: [Register; 26],
    set: [bool; 36], // which numbered/named registers hold data
}

impl Default for Registers {
    fn default() -> Self {
        Self {
            unnamed: Register::default(),
            small_delete: Register::default(),
            numbered: Default::default(),
            named: Default::default(),
            set: [false; 36],
        }
    }
}

/// Index into `Registers::set` for "0-"9 and "a-"z (either case).
fn set_index(name: char) -> Option<usize> {
    match name {
        '0'..='9' => Some(name as usize - '0' as usize),
        'a'..='z' | 'A'..='Z' => {
            Some(10 + (name.to_ascii_lowercase() as usize - 'a' as usize))
        }
        _ => None,
    }
}

impl Registers {
    pub fn is_valid(name: char) -> bool {
        matches!(name, '"' | '-' | '_') || name.is_ascii_alphanumeric()
    }

    fn slot(&self, name: char) -> Option<&Register> {
        match name {
            '"' => Some(&self.unnamed),
            '-' => Some(&self.small_delete),
            _ => match set_index(name)? {
                i @ 0..10 => Some(&self.numbered[i]),
                i => Some(&self.named[i - 10]),
            },
        }
    }

    fn slot_mut(&mut self, name: char) -> Option<&mut Register> {
        match name {
            '"' => Some(&mut self.unnamed),
            '-' => Some(&mut self.small_delete),
            _ => match set_index(name)? {
                i @ 0..10 => Some(&mut self.numbered[i]),
                i => Some(&mut self.named[i - 10]),
            },
        }
    }

    fn write_named(&mut self, name: char, reg: &Register) {
        if name.is_ascii_uppercase() {
            let idx = name.to_ascii_lowercase() as usize - 'a' as usize;
            let dst = &mut self.named[idx];
            if !self.set[10 + idx] {
                *dst = reg.clone();
            } else if dst.linewise || reg.linewise {
                dst.text.push('\n');
                dst.text.push_str(&reg.text);
                dst.linewise = true;
            } else {
                dst.text.push_str(&reg.text);
            }
            self.set[10 + idx] = true;
            return;
        }
        self.set(name, reg);
    }

    /// Store directly (macros); no unnamed/numbered side effects.
    pub fn set(&mut self, name: char, reg: &Register) {
        if let Some(slot) = self.slot_mut(name) {
            *slot = reg.clone();
            if let Some(i) = set_index(name) {
                self.set[i] = true;
            }
        }
    }

    pub fn yank(&mut self, name: char, reg: &Register) {
        if name == '_' {
            return;
        }
        if name == '"' {
            self.set('0', reg);
            self.unnamed = reg.clone();
        } else {
            self.write_named(name, reg);
            self.unnamed = self.slot(name).cloned().unwrap_or_default();
        }
    }

    pub fn del(&mut self, name: char, reg: &Register) {
        if name == '_' {
            return;
        }
        if name != '"' {
            self.write_named(name, reg);
            self.unnamed = self.slot(name).cloned().unwrap_or_default();
            return;
        }
        if reg.linewise || reg.text.contains('\n') {
            // Shift "1 -> "2 -> ... -> "9.
            for i in (2..=9).rev() {
                self.numbered[i] = self.numbered[i - 1].clone();
                self.set[i] = self.set[i - 1];
            }
            self.set('1', reg);
        } else {
            self.small_delete = reg.clone();
        }
        self.unnamed = reg.clone();
    }

    pub fn get(&self, name: char) -> Option<Register> {
        match name {
            '"' | '-' => {
                let r = self.slot(name)?;
                (!r.text.is_empty() || r.linewise).then(|| r.clone())
            }
            _ => {
                let i = set_index(name)?;
                if self.set[i] {
                    self.slot(name).cloned()
                } else {
                    None
                }
            }
        }
    }
}
