use pyo3::prelude::*;

#[pyfunction]
fn thing() -> PyResult<&'static str> {
    Ok("hello from rust")
}

#[pymodule]
fn bar(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(thing, m)?)?;
    Ok(())
}
