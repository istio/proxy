import { Runfiles } from "./runfiles";

// Re-export the `Runfiles` class. This class if the runfile helpers need to be
// mocked for testing purposes. This is used by the linker but also publicly exposed.
export { Runfiles };

/** Instance of the runfile helpers. */
export const runfiles = new Runfiles(process.env);
