```haskell
f :: Int -> Int throws ['OverflowErr a, 'UnderflowErr a, 'DivByZeroErr a]

g :: Int -> Int throws ['OverflowErr a, 'UnderflowErr a]
g = try f catch 'DivByZeroErr a => cdsckdslmcldkcl

main :: IO () throws []

fmap :: (a -> b throws e) -> (f a -> f b throws e)
fmap f [] = []
fmap f (x : xs) = f x : fmap f xs

-- In Haskell
f :: (a -> Either e b)
fmap f :: (f a -> f (Either e b))
dream :: (f a -> Either e (f b))
```