// The entry file of your WebAssembly module.

/*export let globalFloat: f32= 1.123;
export let globalInt: i64= 123;
export const globalConstInt: u32= 0;
export let globalIntWithoutInit: u32;

class SampleClass {
  private x: u32;
  constructor(x: u32) { this.x = x; }
  sayHello() : string { return "sample"; }
};

class SampleDerivedClass extends SampleClass {
  sayHello() : string { return "derived"; }
}

export function subtract(a: i32, b: i32): i32 {
  const mb= -b;
  return add(a, mb);
}

export function add(a: i32, b: i32): i32 {
  return a + b;
}

export function isGreater( a: i32 ) : boolean {
  if( a *2 > 50 ) {
    return true;
  }

  return a < 10;
}
*/
export function calcThing( a: i32 ) : i32 {
  let i= 0
  a= a < 0 ? -a : a;
  while( a > 0 ) {
    i++
    a/=2
  }
  return i
}/*

export function create( x: u32 ) : SampleClass {
  return x ? new SampleClass( x ) : new SampleDerivedClass( x );
}

export function dreck( k: SampleClass ) : string {
  return k.sayHello();
}*/

/*let aGlboalThing: i64= 1234;

export function doSet(x: i64): void {
  aGlboalThing= x;
}

export function doGet(): i64 {
  return aGlboalThing;
}

export function doLocal(x: i64): i64 {
  let value: i64= 0;
  value= x;
  let y= value+ 1;
  return y;
}*/

import { printInt, vecSum } from "./env";

//function printBool(x: boolean): void { printInt( x ? 1 : 0 ); }

// const sum= vecSum(1.0, 2.0, 3.0);
// printFloat(sum);

/*const x: f64 = 3.14;
const y: f64 = 7.65;
const z: f64 = 0.52;

printBool( x == x );
printBool( x != x );
printBool( x == y );
printBool( x != y );
printBool( x < x );
printBool( x < y );
printBool( x > x );
printBool( x > y );*/

/*export function switcher(x: i32): i32 {
  switch(x) {
    case 1: return 31;
    case 2: return 62;
    case 3: return 93;
    case 4: return 124;
    case 5: return 155;
    case 6: return 186;
    case 7: return 217;
    case 8: return 248;
    case 9: return 279;
    case 10: return 310;
    
    default: return -1;
  }
}*/

type fnType= (x:i32) => i32;


function test(x: fnType): i32 {
  return x(3);
}

function a1(x:i32): i32 { return x* 10; }
function a2(x:i32): i32 { return x+ 10; }

export function ttest(a: i32): void {
  let result= test( a > 10 ? a1 : a2 );
  printInt(result);
}

export function fptest(x: f32): i32 {
  return Math.round(x* 2.5 as f32) as i32
}
