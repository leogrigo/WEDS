import joblib
scaler = joblib.load('C:/Users/admin/Documents/IoT/GroupProj/WEDS/LOGS/RiskEvaluation/code/data_scaler_v6f.pkl')
print('Means:', scaler.mean_)
print('Scales:', scaler.scale_)
