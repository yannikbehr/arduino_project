#
# author: Jonas Behr 
# date: Sat Aug 24 15:37:02 2024
######################################

import plotly.express as px
import pandas as pd
import numpy as np
from numpy.polynomial import Chebyshev
#from data_new_op_amp import tmp
from data_pair import tmp_pairs
from numpy.polynomial.polynomial import polyfit
import plotly.graph_objects as go

def calc_temp(z1, z2, split_val, val):
    if val >= split_val:
        z = z2
    else:
        z=z1
    return sum([val**p * z[p] for p in range(len(z))])


def picewise_polynomial_fit(df, col="tmp1", deg=3):
    vals = sorted(list(set(df[col])))
    best_loss = 1e10
    for split_val in vals[10:-10]:
        idx = df[col] <= split_val
        z1 = polyfit(df[idx][col], df[idx]["target"], deg=deg)
        idx = df[col] >= split_val
        z2 = polyfit(df[idx][col], df[idx]["target"], deg=deg)
        df["pred"] = df[col].apply(lambda x: calc_temp(z1, z2, split_val, x))
        loss = sum((df["pred"] - df["target"])**2) / len(df)
        #print(f"split_val: {split_val} loss: {loss}")
        if loss < best_loss:
            best_z1 = z1
            best_z2 = z2
            best_split = split_val
            best_loss = loss
    return {"z1": best_z1, "z2" : best_z2, "split_val": best_split}


df = []
for key in tmp_pairs.keys():
    df1 = pd.DataFrame({
        "tmp0": [x[0] for x in tmp_pairs[key]],
        "tmp1": [x[1] for x in tmp_pairs[key]],
    })
    df1["target"] = key
    df.append(df1)

df = pd.concat(df)

model0 = picewise_polynomial_fit(df, col="tmp0")
model1 = picewise_polynomial_fit(df, col="tmp1")

#tmp_calcs = {}
#for col in ["tmp1", "tmp2"]:
#    tmp_calcs[col] = {
#            "median":{}, 
#            "std":{}, 
#            }
#    z = polyfit(df[col], df["target"], deg=5)
#    print(f"{col}: {z}")
#    tmp_calcs[col]["poly_fit"] = z
#    for key in tmp.keys():
#        tmp_calcs[col]["median"][key] = np.median(df[df['target']==key][col])
#        tmp_calcs[col]["std"][key] = np.std(df[df['target']==key][col])
#        print(f"Temp sensor {col} at temp {key}: mean: {tmp_calcs[col]['median']} std: {tmp_calcs[col]['std']}")

df["pred0"] = df["tmp0"].apply(lambda v: calc_temp(**model0, val=v))
df["pred1"] = df["tmp1"].apply(lambda v: calc_temp(**model1, val=v))

min_val0 = min(list(df["tmp0"]))
max_val0 = max(list(df["tmp0"]))
min_val1 = min(list(df["tmp1"]))
max_val1 = max(list(df["tmp1"]))
x_range0 = np.array(range(int(min_val0 - 10), int(max_val0 + 10)))
x_range1 = np.array(range(int(min_val1 - 10), int(max_val1 + 10)))
y_vals0 = [calc_temp(**model0, val=v) for v in x_range0]
y_vals1 = [calc_temp(**model1, val=v) for v in x_range1]

# plot 1
fig = px.line(x=x_range0, y=y_vals0)
fig.add_trace(go.Scatter(x=x_range1, y=y_vals1, mode="lines"))
fig.add_trace(
    go.Scatter(x=df["tmp0"], y=df[f"target"], mode='markers', name="data_tmp0")
)
fig.add_trace(
    go.Scatter(x=df["tmp1"], y=df[f"target"], mode='markers', name="data_tmp1")
)
fig.show()


# plot 2
fig = px.scatter(df, x="target", y="pred0")
fig.add_trace(go.Scatter(x=df["target"], y=df["pred1"], mode="markers", name="temp1"))
x = np.array([min(df["target"]), max(df["target"])])
fig.add_trace(
    go.Scatter(x=x, y=x, name="slope", line_shape='linear')
)
fig.show()
print(model0)
print(model1)

