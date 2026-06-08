fn main() {
    // this file does not link to any shell32 symbols directly, and
    // will thus cause a compile error if -lalias:shell32
    // is present in the link flags
}
