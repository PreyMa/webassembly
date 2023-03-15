// The entry file of your WebAssembly module.

export let globalFloat: f32= 1.123;
export let globalInt: i64= 123;
export const globalConstInt: u32= 0;
export let globalIntWithoutInit: u32;

class Kek {
  private x: u32;
  constructor(x: u32) { this.x = x; }
  sayHello() : string { return "kek"; }
};

class Speck extends Kek {
  sayHello() : string { return "speck"; }
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

export function calcThing( a: i32 ) : i32 {
  let i= 0
  a= a < 0 ? -a : a;
  while( a > 0 ) {
    i++
    a/=2
  }
  return i
}

/*export function kek( x: u32 ) : Kek {
  return x ? new Kek( x ) : new Speck( x );
}

export function dreck( k: Kek ) : string {
  return k.sayHello();
}*/
