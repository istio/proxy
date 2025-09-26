#[derive(Debug, PartialEq, Eq)]
pub struct LabelError(pub String);

impl std::fmt::Display for LabelError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl std::error::Error for LabelError {
    fn description(&self) -> &str {
        &self.0
    }
}

impl From<String> for LabelError {
    fn from(msg: String) -> Self {
        Self(msg)
    }
}
