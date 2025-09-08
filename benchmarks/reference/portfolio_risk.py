#!/usr/bin/env python3
import math
import random

def portfolio_var(num_assets=3, confidence_level=95):
    """Portfolio Value at Risk calculation"""
    # Portfolio weights
    weights = [0.5, 0.3, 0.2]  # 50% stocks, 30% bonds, 20% commodities
    
    # Expected returns (annualized %)
    expected_returns = [0.12, 0.05, 0.08]
    
    # Volatilities (annualized %)
    volatilities = [0.20, 0.05, 0.25]
    
    # Correlation matrix
    correlations = [
        [1.0, 0.3, 0.5],
        [0.3, 1.0, 0.1], 
        [0.5, 0.1, 1.0]
    ]
    
    # Portfolio expected return
    portfolio_return = sum(w * r for w, r in zip(weights, expected_returns))
    
    # Portfolio variance calculation
    portfolio_variance = 0
    for i in range(3):
        for j in range(3):
            portfolio_variance += weights[i] * weights[j] * volatilities[i] * volatilities[j] * correlations[i][j]
    
    portfolio_vol = math.sqrt(portfolio_variance)
    
    # VaR calculation (assuming normal distribution)
    z_score = 1.645 if confidence_level <= 98 else 2.326
    
    # Daily VaR (1-day, assuming 252 trading days per year)
    daily_vol = portfolio_vol / math.sqrt(252)
    var_95 = z_score * daily_vol
    
    return var_95

def monte_carlo_var(num_simulations=1000):
    """Monte Carlo simulation for VaR"""
    portfolio_value = 1000000  # $1M portfolio
    worst_losses = 0
    count_extreme = 0
    
    random.seed(42)  # For reproducible results
    
    for _ in range(num_simulations):
        # Generate correlated random returns for 3 assets
        rand_nums = [random.gauss(0, 1) for _ in range(3)]
        
        # Simulate daily returns
        returns = [
            0.12 / 252 + 0.20 / math.sqrt(252) * rand_nums[0],  # Stock return
            0.05 / 252 + 0.05 / math.sqrt(252) * rand_nums[1],  # Bond return
            0.08 / 252 + 0.25 / math.sqrt(252) * rand_nums[2]   # Commodity return
        ]
        
        # Portfolio return
        portfolio_return = 0.5 * returns[0] + 0.3 * returns[1] + 0.2 * returns[2]
        
        # Portfolio loss (negative return)
        if portfolio_return < 0:
            loss = abs(portfolio_return * portfolio_value)
            worst_losses += loss
            count_extreme += 1
    
    # Average loss in extreme scenarios
    if count_extreme > 0:
        avg_extreme_loss = worst_losses / count_extreme
    else:
        avg_extreme_loss = 0
    
    return avg_extreme_loss

def main():
    """Portfolio Risk Analysis Benchmark"""
    total_var = 0
    total_mc_var = 0
    
    # Run multiple VaR calculations with different parameters
    for i in range(1000):
        # Parametric VaR
        confidence = 95 + (i % 5)  # Confidence levels 95-99%
        var_result = portfolio_var(3, confidence)
        total_var += var_result
        
        # Monte Carlo VaR (smaller simulation for speed)
        mc_result = monte_carlo_var(1000)
        total_mc_var += mc_result
    
    avg_var = total_var / 1000
    avg_mc_var = total_mc_var / 1000
    
    # Combined risk measure
    combined_risk = (avg_var + avg_mc_var / 1000000) * 1000  # Scale for comparison
    
    print(combined_risk)
    return combined_risk

if __name__ == "__main__":
    main()