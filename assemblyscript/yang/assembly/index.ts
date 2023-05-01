// The entry file of your WebAssembly module.

export function a(): void {}
import { a1, a3, a5 } from './ying';
export { a as a0, a1 as a2, a3 as a4 };

//import { cycle } from './ying';
//export { cycle };

// export function nonVoidFunction(x:i32): i32 { return x+1; }

export const c: f64= 3.14;
import { c1, c3, c5 } from './ying';
export { c as c0, c1 as c2, c3 as c4 };

export function test(): void {
  a5();
  let cc= c5;
}

export function add(a: i32, b: i32): i32 {
  return a + b;
}
