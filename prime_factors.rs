#[no_mangle]
pub fn prime_factors(mut n: u64) -> Vec<u64> {
    let mut factors = Vec::new();
    let mut candidate = 2;

    while n > 1 {
        while n % candidate == 0 {
            factors.push(candidate);
            n /= candidate;
        }
        candidate += 1;
    }

    factors
}
