// Additional source code for `a.proto`.

use std::fmt::{Display, Formatter, Result};

impl Display for crate::a::A {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result {
        write!(f, "Display of: A")
    }
}
