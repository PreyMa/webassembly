// The entry file of your WebAssembly module.

export function a(): void {}

import { a0, a2, a4 } from './yang';

export { a0 as a1, a2 as a3, a4 as a5 };

import { cycle } from './yang';
export { cycle };
