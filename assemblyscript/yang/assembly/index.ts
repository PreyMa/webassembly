// The entry file of your WebAssembly module.

export function a(): void {}

import { a1, a3, a5 } from './ying';

export { a as a0, a1 as a2, a3 as a4 };

//import { cycle } from './ying';
//export { cycle };

export function test(): void {
  a5();
}

export function add(a: i32, b: i32): i32 {
  return a + b;
}

export function nonVoidFunction(x:i32): i32 { return x+1; }
