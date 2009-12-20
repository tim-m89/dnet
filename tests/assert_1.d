int foo(int a, int b)  
{                      
   if (a == b)         
   {                   
       assert(a);      
       return b;       
   }                   
   else                
   {                   
       assert(b);      
       return a + b;   
   }                   
}                      
int main() {           
   int i = foo(0, 0);  
   return foo(i, 1);   
}